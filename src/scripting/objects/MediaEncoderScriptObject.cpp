#include "MediaEncoderScriptObject.h"

#if defined(MULTIMEDIA_ENABLED)

#  include "scripting/ScriptTimeout.h"
#  include <QAudioBuffer>
#  include <QAudioBufferInput>
#  include <QAudioFormat>
#  include <QCoreApplication>
#  include <QElapsedTimer>
#  include <QJsonObject>
#  include <QThread>
#  include <QFileInfo>
#  include <QMediaCaptureSession>
#  include <QMediaFormat>
#  include <QMediaRecorder>
#  include <QMetaEnum>
#  include <QMimeType>
#  include <QTimer>
#  include <QUrl>
#  include <QVideoFrame>
#  include <QVideoFrameFormat>
#  include <QVideoFrameInput>
#  include <algorithm>
#  include <optional>
#  include <vector>

namespace {
    struct AudioCodecConfiguration
    {
        QMediaFormat::AudioCodec value{ };
        QString key;
        QString name;
        bool isDefault{ };
    };

    struct VideoCodecConfiguration
    {
        QMediaFormat::VideoCodec value{ };
        QString key;
        QString name;
        std::vector<AudioCodecConfiguration> audioCodecs;
        bool isDefault{ };
    };

    struct FormatConfiguration
    {
        QMediaFormat::FileFormat value{ };
        QString key;
        QString name;
        QString suffix;
        std::vector<VideoCodecConfiguration> videoCodecs;
        std::vector<AudioCodecConfiguration> audioCodecs;
        bool isDefault{ };
    };

    template <typename Enum>
    QString enumKey(Enum value)
    {
        const auto key =
            QMetaEnum::fromType<Enum>().valueToKey(static_cast<int>(value));
        return key ? QString::fromLatin1(key) : QString();
    }

    template <typename Enum>
    std::optional<Enum> enumValue(const QString &key)
    {
        if (key.isEmpty())
            return std::nullopt;
        auto ok = false;
        const auto value = QMetaEnum::fromType<Enum>().keyToValue(
            key.toLatin1().constData(), &ok);
        if (!ok)
            return std::nullopt;
        return static_cast<Enum>(value);
    }

    QString suffixForFormat(QMediaFormat::FileFormat fileFormat)
    {
        if (fileFormat == QMediaFormat::MP3)
            return "mp3";
        const auto suffix =
            QMediaFormat(fileFormat).mimeType().preferredSuffix();
        if (!suffix.isEmpty())
            return suffix;

        using F = QMediaFormat::FileFormat;
        switch (fileFormat) {
        case F::Matroska:  return "mkv";
        case F::MPEG4:     return "mp4";
        case F::WebM:      return "webm";
        case F::AVI:       return "avi";
        case F::QuickTime: return "mov";
        default:           return "media";
        }
    }

    template <typename CodecConfiguration>
    void chooseCodecDefault(std::vector<CodecConfiguration> &codecs,
        const QStringList &preferences)
    {
        if (codecs.empty())
            return;
        auto selected = codecs.begin();
        for (const auto &preference : preferences) {
            const auto it = std::find_if(codecs.begin(), codecs.end(),
                [&](const auto &codec) { return codec.key == preference; });
            if (it != codecs.end()) {
                selected = it;
                break;
            }
        }
        selected->isDefault = true;
    }

    std::vector<FormatConfiguration> probeConfigurations()
    {
        auto result = std::vector<FormatConfiguration>();
        auto probe = QMediaFormat();
        for (const auto fileFormat :
            probe.supportedFileFormats(QMediaFormat::Encode)) {
            auto configuration = FormatConfiguration{
                .value = fileFormat,
                .key = enumKey(fileFormat),
                .name = QMediaFormat::fileFormatDescription(fileFormat),
                .suffix = suffixForFormat(fileFormat),
            };
            if (configuration.key.isEmpty())
                continue;

            auto format = QMediaFormat(fileFormat);
            for (const auto videoCodec :
                format.supportedVideoCodecs(QMediaFormat::Encode)) {
                auto configuredFormat = QMediaFormat(fileFormat);
                configuredFormat.setVideoCodec(videoCodec);
                if (!configuredFormat.isSupported(QMediaFormat::Encode))
                    continue;
                auto video = VideoCodecConfiguration{
                    .value = videoCodec,
                    .key = enumKey(videoCodec),
                    .name = QMediaFormat::videoCodecDescription(videoCodec),
                };
                if (video.key.isEmpty())
                    continue;
                for (const auto audioCodec :
                    configuredFormat.supportedAudioCodecs(
                        QMediaFormat::Encode)) {
                    auto combinedFormat = configuredFormat;
                    combinedFormat.setAudioCodec(audioCodec);
                    const auto key = enumKey(audioCodec);
                    if (!key.isEmpty()
                        && combinedFormat.isSupported(QMediaFormat::Encode)) {
                        video.audioCodecs.push_back({
                            .value = audioCodec,
                            .key = key,
                            .name =
                                QMediaFormat::audioCodecDescription(audioCodec),
                        });
                    }
                }
                chooseCodecDefault(video.audioCodecs, { "MP3", "AAC" });
                configuration.videoCodecs.push_back(std::move(video));
            }
            chooseCodecDefault(configuration.videoCodecs, { "H264" });

            for (const auto audioCodec :
                format.supportedAudioCodecs(QMediaFormat::Encode)) {
                auto configuredFormat = QMediaFormat(fileFormat);
                configuredFormat.setAudioCodec(audioCodec);
                const auto key = enumKey(audioCodec);
                if (!key.isEmpty()
                    && configuredFormat.isSupported(QMediaFormat::Encode)) {
                    configuration.audioCodecs.push_back({
                        .value = audioCodec,
                        .key = key,
                        .name = QMediaFormat::audioCodecDescription(audioCodec),
                    });
                }
            }
            chooseCodecDefault(configuration.audioCodecs, { "MP3", "AAC" });
            if (!configuration.videoCodecs.empty()
                || !configuration.audioCodecs.empty()) {
                result.push_back(std::move(configuration));
            }
        }
        return result;
    }

    void chooseFormatDefault(std::vector<FormatConfiguration> &formats,
        bool video)
    {
        auto selected = static_cast<FormatConfiguration *>(nullptr);
        auto selectedScore = -1;
        for (auto &candidate : formats) {
            const auto isVideo = !candidate.videoCodecs.empty();
            if (video != isVideo || (!video && candidate.audioCodecs.empty()))
                continue;
            auto format = &candidate;
            auto score = 0;
            if (format->key == "MPEG4")
                score += video ? 100 : 20;
            if (format->key == "MP3")
                score += video ? 0 : 100;
            if (video) {
                const auto codec = std::find_if(format->videoCodecs.begin(),
                    format->videoCodecs.end(),
                    [](const auto &entry) { return entry.isDefault; });
                if (codec != format->videoCodecs.end()) {
                    if (codec->key == "H264")
                        score += 1000;
                    if (std::ranges::any_of(codec->audioCodecs,
                            [](const auto &entry) {
                                return entry.key == "MP3";
                            })) {
                        score += 200;
                    } else if (!codec->audioCodecs.empty()) {
                        score += 50;
                    }
                }
            } else if (
                std::ranges::any_of(format->audioCodecs,
                    [](const auto &entry) { return entry.key == "MP3"; })) {
                score += 1000;
            }
            if (!selected || score > selectedScore) {
                selected = format;
                selectedScore = score;
            }
        }
        if (selected)
            selected->isDefault = true;
    }

    QJsonObject audioCodecsJson(
        const std::vector<AudioCodecConfiguration> &codecs)
    {
        auto result = QJsonObject();
        for (const auto &codec : codecs) {
            auto object = QJsonObject{
                { "name", codec.name },
            };
            if (codec.isDefault)
                object.insert("default", true);
            result.insert(codec.key, object);
        }
        return result;
    }

    QJsonObject formatsJson()
    {
        auto formats = probeConfigurations();
        chooseFormatDefault(formats, true);
        chooseFormatDefault(formats, false);

        auto result = QJsonObject();
        for (const auto &format : formats) {
            auto formatObject = QJsonObject{
                { "name", format.name },
                { "suffix", format.suffix },
            };
            if (!format.videoCodecs.empty()) {
                auto videoCodecs = QJsonObject();
                for (const auto &codec : format.videoCodecs) {
                    auto codecObject = QJsonObject{
                        { "name", codec.name },
                        { "audioCodecs", audioCodecsJson(codec.audioCodecs) },
                    };
                    if (codec.isDefault)
                        codecObject.insert("default", true);
                    videoCodecs.insert(codec.key, codecObject);
                }
                formatObject.insert("videoCodecs", videoCodecs);
            } else {
                formatObject.insert("audioCodecs",
                    audioCodecsJson(format.audioCodecs));
            }
            if (format.isDefault)
                formatObject.insert("default", true);
            result.insert(format.key, formatObject);
        }
        return result;
    }
} // namespace

class MediaEncoderScriptObject::State
{
public:
    State(MediaEncoderScriptObject &object, const QVariantMap &options)
        : mObject(object)
        , mDestination(
              QFileInfo(options.value("outputFile").toString().trimmed())
                  .absoluteFilePath())
        , mFrameRate(
              std::max(0.001, options.value("videoFrameRate", 60.0).toDouble()))
        , mVideoBitRate(
              std::max(1, options.value("videoBitRate", 12'000'000).toInt()))
        , mAudioBitRate(
              std::max(1, options.value("audioBitRate", 192'000).toInt()))
    {
        const auto fileFormat = enumValue<QMediaFormat::FileFormat>(
            options.value("fileFormat").toString());
        const auto videoCodec = enumValue<QMediaFormat::VideoCodec>(
            options.value("videoCodec").toString());
        const auto audioCodec = enumValue<QMediaFormat::AudioCodec>(
            options.value("audioCodec").toString());
        if (!fileFormat) {
            mStartupError = mObject.tr("The media format is invalid.");
            return;
        }
        mFileFormat = *fileFormat;
        if (videoCodec) {
            mVideoCodec = *videoCodec;
            mHasVideo = true;
        }
        if (audioCodec) {
            mAudioCodec = *audioCodec;
            mHasAudio = true;
        }
        mVideoEosSent = !mHasVideo;
        mAudioEosSent = !mHasAudio;
        if (!mHasVideo && !mHasAudio) {
            mStartupError =
                mObject.tr("The media encoder needs a video or audio codec.");
            return;
        }

        auto mediaFormat = QMediaFormat(mFileFormat);
        if (mHasVideo)
            mediaFormat.setVideoCodec(mVideoCodec);
        if (mHasAudio)
            mediaFormat.setAudioCodec(mAudioCodec);
        if (!mediaFormat.isSupported(QMediaFormat::Encode)) {
            mStartupError = mObject.tr(
                "The selected format and codec combination is not "
                "supported.");
            return;
        }
        if (options.value("outputFile").toString().trimmed().isEmpty()) {
            mStartupError = mObject.tr("Choose an output file.");
            return;
        }
    }

    ~State() { disposeMediaObjects(); }

    bool completed() const { return mCompleted; }

    void writeFrame(MediaFrame frame)
    {
        if (mCompleted)
            return;
        if (!mStartupError.isEmpty())
            return fail(mStartupError);
        if (mFrameInProgress)
            return fail(mObject.tr(
                "The previous media frame has not been written yet."));
        if (!frame.errorString().isEmpty())
            return fail(frame.errorString());
        if (mHasVideo && frame.image().isNull())
            return fail(mObject.tr("The media frame has no video image."));
        if (mHasAudio) {
            const auto sampleRate = frame.audioSampleRate();
            if (sampleRate <= 0
                || frame.audioSamples().size() % BytesPerFrame != 0
                || (mAudioFormat.isValid()
                    && sampleRate != mAudioFormat.sampleRate())) {
                return fail(mObject.tr(
                    "The media frame has an unexpected audio format."));
            }
            if (!mAudioFormat.isValid()) {
                mAudioFormat.setSampleRate(sampleRate);
                mAudioFormat.setChannelCount(ChannelCount);
                mAudioFormat.setSampleFormat(QAudioFormat::Float);
            }
        }

        if (!mRecorder && !initialize(frame.image().size()))
            return;
        if (mHasVideo && frame.image().size() != mVideoSize) {
            return fail(mObject
                    .tr("The video resolution changed while encoding (%1 × "
                        "%2).")
                    .arg(frame.image().width())
                    .arg(frame.image().height()));
        }

        mFrameInProgress = true;
        mFrameIsFinish = frame.isFinish();
        if (mHasVideo) {
            mPendingVideo = QVideoFrame(frame.image());
            mPendingVideo.setStartTime(frame.videoStartTime());
            mPendingVideo.setEndTime(frame.videoEndTime());
            mPendingVideo.setStreamFrameRate(mFrameRate);
            mHasPendingVideo = true;
        }
        if (mHasAudio && !frame.audioSamples().isEmpty()) {
            mPendingAudio = QAudioBuffer(frame.audioSamples(), mAudioFormat,
                frame.audioStartTime());
            mHasPendingAudio = true;
        }
        tryAdvance();
    }

    void close()
    {
        if (mCompleted)
            return;
        disposeMediaObjects();
        mCompleted = true;
    }

private:
    bool initialize(QSize videoSize)
    {
        mVideoSize = videoSize;
        mCaptureSession = std::make_unique<QMediaCaptureSession>();
        mRecorder = std::make_unique<QMediaRecorder>();
        mCaptureSession->setRecorder(mRecorder.get());

        if (mHasVideo) {
            if (mVideoSize.isEmpty()) {
                fail(mObject.tr("The video resolution is invalid."));
                return false;
            }
            mVideoInput = std::make_unique<QVideoFrameInput>(QVideoFrameFormat(
                mVideoSize, QVideoFrameFormat::Format_RGBA8888));
            mCaptureSession->setVideoFrameInput(mVideoInput.get());
            QObject::connect(mVideoInput.get(),
                &QVideoFrameInput::readyToSendVideoFrame, &mObject,
                [this] { tryAdvance(); });
        }
        if (mHasAudio) {
            mAudioInput = std::make_unique<QAudioBufferInput>(mAudioFormat);
            mCaptureSession->setAudioBufferInput(mAudioInput.get());
            QObject::connect(mAudioInput.get(),
                &QAudioBufferInput::readyToSendAudioBuffer, &mObject,
                [this] { tryAdvance(); });
        }
        QObject::connect(mRecorder.get(), &QMediaRecorder::errorOccurred,
            &mObject, [this](QMediaRecorder::Error, const QString &message) {
                const auto error = message.isEmpty()
                    ? mObject.tr("Encoding failed.")
                    : message;
                QTimer::singleShot(0, &mObject, [this, error] { fail(error); });
            });
        QObject::connect(mRecorder.get(), &QMediaRecorder::recorderStateChanged,
            &mObject, [this](QMediaRecorder::RecorderState state) {
                if (state != QMediaRecorder::StoppedState)
                    return;
                QTimer::singleShot(0, &mObject, [this] {
                    if (mCompleted)
                        return;
                    if (mRecorder
                        && mRecorder->error() != QMediaRecorder::NoError) {
                        fail(mRecorder->errorString().isEmpty()
                                ? mObject.tr("Encoding failed.")
                                : mRecorder->errorString());
                    } else if (mAborting) {
                        completeFailure(mFailureMessage);
                    } else if (mFinalizing) {
                        finishSuccessfully();
                    }
                });
            });
        auto mediaFormat = QMediaFormat(mFileFormat);
        if (mHasVideo)
            mediaFormat.setVideoCodec(mVideoCodec);
        if (mHasAudio)
            mediaFormat.setAudioCodec(mAudioCodec);
        mRecorder->setMediaFormat(mediaFormat);
        mRecorder->setOutputLocation(QUrl::fromLocalFile(mDestination));
        if (mHasVideo) {
            mRecorder->setVideoResolution(mVideoSize);
            mRecorder->setVideoFrameRate(mFrameRate);
            mRecorder->setVideoBitRate(mVideoBitRate);
        }
        if (mHasAudio) {
            mRecorder->setAudioSampleRate(mAudioFormat.sampleRate());
            mRecorder->setAudioChannelCount(ChannelCount);
            mRecorder->setAudioBitRate(mAudioBitRate);
        }
        mRecorder->setAutoStop(true);
        mRecorder->record();
        return true;
    }

    void tryAdvance()
    {
        if (mCompleted || mAdvancing || mAborting || !mFrameInProgress)
            return;
        mAdvancing = true;
        if (mHasPendingVideo && mVideoInput->sendVideoFrame(mPendingVideo)) {
            mPendingVideo = { };
            mHasPendingVideo = false;
        }
        if (mHasPendingAudio && mAudioInput->sendAudioBuffer(mPendingAudio)) {
            mPendingAudio = { };
            mHasPendingAudio = false;
        }
        if (mHasPendingVideo || mHasPendingAudio) {
            mAdvancing = false;
            return;
        }

        if (!mFrameIsFinish) {
            mFrameInProgress = false;
            mAdvancing = false;
            QTimer::singleShot(0, &mObject, [this] {
                if (!mCompleted && !mFrameInProgress)
                    Q_EMIT mObject.frameWritten();
            });
            return;
        }

        mFinalizing = true;
        if (mHasVideo && !mVideoEosSent
            && mVideoInput->sendVideoFrame(QVideoFrame())) {
            mVideoEosSent = true;
        }
        if (mHasAudio && !mAudioEosSent
            && mAudioInput->sendAudioBuffer(QAudioBuffer())) {
            mAudioEosSent = true;
        }
        mAdvancing = false;
    }

    void fail(QString message)
    {
        if (mCompleted || mAborting)
            return;
        mFailureMessage = std::move(message);
        mAborting = true;
        mFinalizing = false;
        if (mRecorder
            && mRecorder->recorderState() != QMediaRecorder::StoppedState) {
            mRecorder->stop();
        } else {
            completeFailure(mFailureMessage);
        }
    }

    void finishSuccessfully()
    {
        if (mCompleted || mAborting)
            return;
        disposeMediaObjects();
        mCompleted = true;
        Q_EMIT mObject.finished({ });
    }

    void completeFailure(QString message)
    {
        if (mCompleted)
            return;
        mCompleted = true;
        disposeMediaObjects();
        Q_EMIT mObject.finished(std::move(message));
    }

    void disposeMediaObjects()
    {
        if (mRecorder)
            QObject::disconnect(mRecorder.get(), nullptr, &mObject, nullptr);
        if (mVideoInput)
            QObject::disconnect(mVideoInput.get(), nullptr, &mObject, nullptr);
        if (mAudioInput)
            QObject::disconnect(mAudioInput.get(), nullptr, &mObject, nullptr);
        if (mCaptureSession) {
            mCaptureSession->setRecorder(nullptr);
            if (mHasVideo)
                mCaptureSession->setVideoFrameInput(nullptr);
            if (mHasAudio)
                mCaptureSession->setAudioBufferInput(nullptr);
        }
        mAudioInput.reset();
        mVideoInput.reset();
        mRecorder.reset();
        mCaptureSession.reset();
    }

    MediaEncoderScriptObject &mObject;
    QString mDestination;
    QString mStartupError;
    QString mFailureMessage;
    double mFrameRate{ };
    int mVideoBitRate{ };
    int mAudioBitRate{ };
    QMediaFormat::FileFormat mFileFormat{ QMediaFormat::UnspecifiedFormat };
    QMediaFormat::VideoCodec mVideoCodec{
        QMediaFormat::VideoCodec::Unspecified
    };
    QMediaFormat::AudioCodec mAudioCodec{
        QMediaFormat::AudioCodec::Unspecified
    };
    QSize mVideoSize;
    QAudioFormat mAudioFormat;
    std::unique_ptr<QMediaCaptureSession> mCaptureSession;
    std::unique_ptr<QMediaRecorder> mRecorder;
    std::unique_ptr<QVideoFrameInput> mVideoInput;
    std::unique_ptr<QAudioBufferInput> mAudioInput;
    QVideoFrame mPendingVideo;
    QAudioBuffer mPendingAudio;
    bool mHasVideo{ };
    bool mHasAudio{ };
    bool mHasPendingVideo{ };
    bool mHasPendingAudio{ };
    bool mFrameInProgress{ };
    bool mFrameIsFinish{ };
    bool mAdvancing{ };
    bool mFinalizing{ };
    bool mAborting{ };
    bool mCompleted{ };
    bool mVideoEosSent{ };
    bool mAudioEosSent{ };
};

MediaEncoderScriptObject::MediaEncoderScriptObject(const QVariantMap &options,
    QObject *parent)
    : QObject(parent)
    , mState(std::make_unique<State>(*this, options))
{
}

MediaEncoderScriptObject::~MediaEncoderScriptObject() = default;

QJsonObject MediaEncoderScriptObject::formats()
{
    return formatsJson();
}

void MediaEncoderScriptObject::writeFrame(MediaFrame frame)
{
    mState->writeFrame(std::move(frame));
}

bool MediaEncoderScriptObject::waitForFinished(int timeout)
{
    const auto guard = suspendScriptEngineTimeout();
    auto timer = QElapsedTimer();
    timer.start();
    while (!mState->completed()) {
        if (timer.elapsed() >= timeout)
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return true;
}

void MediaEncoderScriptObject::close()
{
    mState->close();
}

#else // !defined(MULTIMEDIA_ENABLED)

class MediaEncoderScriptObject::State
{
};

MediaEncoderScriptObject::MediaEncoderScriptObject(const QVariantMap &,
    QObject *parent)
    : QObject(parent)
    , mState(std::make_unique<State>())
{
}

MediaEncoderScriptObject::~MediaEncoderScriptObject() = default;

QJsonObject MediaEncoderScriptObject::formats()
{
    return { };
}

void MediaEncoderScriptObject::writeFrame(MediaFrame)
{
    Q_EMIT finished(tr("Media encoding is not available."));
}

bool MediaEncoderScriptObject::waitForFinished(int)
{
    return true;
}

void MediaEncoderScriptObject::close() { }

#endif // !defined(MULTIMEDIA_ENABLED)
