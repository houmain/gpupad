#include "DecodedAudio.h"
#include "MediaSource.h"

#if defined(MULTIMEDIA_ENABLED)

#  include <QAudioBuffer>
#  include <QAudioDecoder>
#  include <QAudioFormat>
#  include <QFileInfo>
#  include <QUrl>
#  include <algorithm>
#  include <cstring>
#  include <utility>

DecodedAudio::DecodedAudio(QString fileName, int sampleRate)
    : mDecoder(new QAudioDecoder(this))
    , mFileName(std::move(fileName))
    , mSampleRate(std::max(sampleRate, 1))
{
    mDecoder->setSource(QUrl::fromLocalFile(mFileName));

    connect(mDecoder, &QAudioDecoder::bufferReady, this,
        &DecodedAudio::handleBuffer);
    connect(mDecoder, &QAudioDecoder::finished, this,
        [this] { finish(); });
    connect(mDecoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error),
        this, [this](QAudioDecoder::Error error) {
            if (error == QAudioDecoder::NoError)
                return;
            auto message = mDecoder->errorString();
            if (message.isEmpty())
                message = tr("Could not decode %1.")
                              .arg(QFileInfo(mFileName).fileName());
            finish(std::move(message));
        });

    auto format = QAudioFormat();
    format.setSampleRate(mSampleRate);
    format.setChannelCount(ChannelCount);
    format.setSampleFormat(QAudioFormat::Float);
    mDecoder->setAudioFormat(format);
    if (!mDecoder->isSupported()) {
        finish(tr("Audio decoding is not supported by this Qt multimedia "
                  "backend."));
        return;
    }
    mDecoder->start();
}

void DecodedAudio::handleBuffer()
{
    if (mFinished)
        return;
    const auto buffer = mDecoder->read();
    if (!buffer.isValid())
        return;

    const auto format = buffer.format();
    if (format.sampleRate() != mSampleRate
        || format.channelCount() != 2
        || format.sampleFormat() != QAudioFormat::Float) {
        finish(tr("The Qt multimedia backend could not convert %1 to the "
                  "audio mixing format.")
                   .arg(QFileInfo(mFileName).fileName()));
        mDecoder->stop();
        return;
    }
    mData.append(buffer.constData<const char>(), buffer.byteCount());
}

void DecodedAudio::finish(QString error)
{
    if (std::exchange(mFinished, true))
        return;
    if (error.isEmpty() && mData.isEmpty())
        error = tr("No audio samples were decoded from %1.")
                    .arg(QFileInfo(mFileName).fileName());
    if (!error.isEmpty())
        mData.clear();
    Q_EMIT finished();
}

QByteArray DecodedAudio::samples(uint64_t sampleBase, int frameCount) const
{
    if (!mFinished || frameCount <= 0)
        return { };

    const auto sourceFrameCount = mData.size() / BytesPerFrame;
    if (sourceFrameCount <= 0)
        return { };

    auto output = QByteArray(frameCount * BytesPerFrame, Qt::Uninitialized);
    auto outputFrame = qsizetype{ 0 };
    auto sourceFrame = static_cast<qsizetype>(
        sampleBase % static_cast<uint64_t>(sourceFrameCount));
    const auto requestedFrameCount = static_cast<qsizetype>(frameCount);
    while (outputFrame < requestedFrameCount) {
        const auto copyFrameCount = std::min(requestedFrameCount - outputFrame,
            sourceFrameCount - sourceFrame);
        std::memcpy(output.data() + outputFrame * BytesPerFrame,
            mData.constData() + sourceFrame * BytesPerFrame,
            static_cast<size_t>(copyFrameCount * BytesPerFrame));
        outputFrame += copyFrameCount;
        sourceFrame = 0;
    }
    return output;
}

#endif // defined(MULTIMEDIA_ENABLED)
