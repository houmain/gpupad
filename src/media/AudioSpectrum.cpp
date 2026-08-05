#include "AudioSpectrum.h"

#if defined(MULTIMEDIA_ENABLED)

#  include "TextureData.h"
#  include <QAudioBuffer>
#  include <QAudioBufferOutput>
#  include <QAudioFormat>
#  include <QAudioOutput>
#  include <QUrl>
#  include <algorithm>
#  include <cmath>
#  include <cstring>
#  include <utility>

namespace {
    constexpr auto SeekThreshold = std::chrono::milliseconds(100);
}

AudioSpectrum::AudioSpectrum(QString fileName, bool flipVertically,
    QObject *parent)
    : VideoStream(fileName, flipVertically, parent)
{
    mPlayer = new QMediaPlayer(this);
    mAudioOutput = new QAudioOutput(this);
    mBufferOutput = new QAudioBufferOutput(this);

    connect(mPlayer, &QMediaPlayer::mediaStatusChanged, this,
        &AudioSpectrum::handleStatusChanged);
    connect(mPlayer, &QMediaPlayer::errorOccurred, this,
        [this](QMediaPlayer::Error error, const QString &) {
            if (error != QMediaPlayer::NoError)
                failLoading();
        });
    connect(mBufferOutput, &QAudioBufferOutput::audioBufferReceived, this,
        &AudioSpectrum::handleAudioBuffer);

    mPlayer->setAudioOutput(mAudioOutput);
    mPlayer->setAudioBufferOutput(mBufferOutput);
    mPlayer->setLoops(QMediaPlayer::Infinite);
    mPlayer->setSource(QUrl::fromLocalFile(fileName));
}

void AudioSpectrum::handleStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::InvalidMedia) {
        failLoading();
    } else if (status == QMediaPlayer::LoadedMedia) {
        if (mPlayer->hasAudio() && mPlayer->duration() > 0)
            publishSpectrum();
        else
            failLoading();
    } else if (status == QMediaPlayer::EndOfMedia) {
        resetAnalysis();
    }
}

void AudioSpectrum::handleAudioBuffer(const QAudioBuffer &buffer)
{
    if (!buffer.isValid()) {
        resetAnalysis();
        return;
    }

    const auto format = buffer.format();
    const auto channelCount = format.channelCount();
    const auto bytesPerSample = format.bytesPerSample();
    if (!format.isValid() || channelCount <= 0 || bytesPerSample <= 0)
        return;

    const auto data = buffer.constData<const char>();
    const auto bytesPerFrame = format.bytesPerFrame();
    for (auto frame = qsizetype{ }; frame < buffer.frameCount(); ++frame) {
        const auto frameData = data + frame * bytesPerFrame;
        auto sample = 0.0f;
        for (auto channel = 0; channel < channelCount; ++channel)
            sample += format.normalizedSampleValue(
                frameData + channel * bytesPerSample);
        appendSample(sample / channelCount);
    }

    if (mSampleCount == SignalSize
        && mSamplesSinceSpectrum >= SpectrumHopSize) {
        mSamplesSinceSpectrum %= SpectrumHopSize;
        publishSpectrum();
    }
}

void AudioSpectrum::appendSample(float sample)
{
    mSignal[mWriteIndex] = sample;
    mWriteIndex = (mWriteIndex + 1) % SignalSize;
    mSampleCount = std::min(mSampleCount + 1, SignalSize);
    ++mSamplesSinceSpectrum;
}

void AudioSpectrum::publishSpectrum()
{
    for (auto i = 0; i < SignalSize; ++i)
        mOrderedSignal[i] = mSignal[(mWriteIndex + i) % SignalSize];

    mFft.set_signal(mOrderedSignal.data());
    const auto &amplitudes = mFft.amplitudes();

    auto texture = TextureData{ };
    if (!texture.create(Texture::Target::Target2D, Texture::Format::R32F,
            static_cast<int>(amplitudes.size()), 1, 1, 1, 1))
        return;
    std::memcpy(texture.getWriteonlyData(), amplitudes.data(),
        amplitudes.size() * sizeof(float));
    presentTexture(std::move(texture));
}

void AudioSpectrum::seek(std::chrono::milliseconds time)
{
    if (!mPlayer)
        return;

    if (mTargetTime == time) {
        mPlayer->pause();
        return;
    }
    mTargetTime = time;

    const auto position = std::chrono::milliseconds(mPlayer->position());
    const auto duration = std::chrono::milliseconds(mPlayer->duration());
    const auto target = mTargetTime % duration;
    const auto distance = abs(position - target);
    if (distance > SeekThreshold) {
        resetAnalysis();
        mPlayer->setPosition(std::chrono::milliseconds(target).count());
    }
    if (!mPlayer->isPlaying())
        mPlayer->play();
}

void AudioSpectrum::resetAnalysis()
{
    mWriteIndex = 0;
    mSampleCount = 0;
    mSamplesSinceSpectrum = 0;
}

void AudioSpectrum::failLoading()
{
    if (mPlayer)
        mPlayer->deleteLater();
    mPlayer = nullptr;
    Q_EMIT loadingFinished();
}

#endif // defined(MULTIMEDIA_ENABLED)
