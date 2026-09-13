#include "SoundOutput.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#if defined(MULTIMEDIA_ENABLED)

#  include <QAudioDevice>
#  include <QAudioFormat>
#  include <QAudioSink>
#  include <QIODevice>
#  include <QMediaDevices>

namespace {
    inline constexpr auto MinimumBufferDurationMilliseconds = 50;
    inline constexpr auto MaximumBufferDurationMilliseconds = 1000;
    inline constexpr auto MinimumSampleRate = 8'000;
    inline constexpr auto MaximumSampleRate = 384'000;
    inline constexpr auto MinimumChunkFrameCount = 1;
    inline constexpr auto MaximumChunkFrameCount = 65'536;
    inline constexpr auto MaximumChunksPerEvaluation = 64;
} // namespace

SoundOutput::SoundOutput(QObject *parent) : QObject(parent) { }

SoundOutput::~SoundOutput()
{
    stop();
}

void SoundOutput::synchronizeToAppTime()
{
    mSynchronizeToAppTime = true;
}

void SoundOutput::setBufferDuration(int milliseconds)
{
    milliseconds = std::clamp(milliseconds, MinimumBufferDurationMilliseconds,
        MaximumBufferDurationMilliseconds);
    if (std::exchange(mBufferDurationMilliseconds, milliseconds)
        != milliseconds) {
        synchronizeToAppTime();
    }
}

void SoundOutput::setFormat(int sampleRate, int chunkFrameCount)
{
    sampleRate = std::clamp(sampleRate, MinimumSampleRate, MaximumSampleRate);
    chunkFrameCount = std::clamp(chunkFrameCount, MinimumChunkFrameCount,
        MaximumChunkFrameCount);
    const auto changed = std::exchange(mSampleRate, sampleRate) != sampleRate
        || mChunkFrameCount != chunkFrameCount;
    mChunkFrameCount = chunkFrameCount;
    if (changed)
        synchronizeToAppTime();
}

void SoundOutput::setAppTime(double appTime, bool reset)
{
    if (std::exchange(mSynchronizeToAppTime, false) || reset)
        start(appTime);
}

void SoundOutput::setPlaying(bool playing)
{
    const auto wasPlaying = std::exchange(mPlaying, playing);
    if (playing && !wasPlaying) {
        stop();
        mSynchronizeToAppTime = true;
    }
    if (mSink) {
        if (playing) {
            mSink->resume();
            stream();
        } else {
            mSink->suspend();
        }
    }
}

int SoundOutput::requestedChunkCount(bool hasAudio)
{
    stream();
    if (!hasAudio) {
        mHasAudio = false;
        return 0;
    }
    if (!mPlaying || !mSink)
        return 0;

    const auto processedFrameCount =
        (mDevice ? (mSink->processedUSecs() * mSampleRate) / 1'000'000 : 0);
    const auto generatedFrameCount = mSampleBase - mStartSampleBase;
    const auto desiredFrameCount = mTargetBufferedFrameCount
        + processedFrameCount;
    const auto missingFrameCount = desiredFrameCount - generatedFrameCount;
    const auto chunkFrameCount = mChunkFrameCount;
    const auto chunkCount = (missingFrameCount + chunkFrameCount - 1)
        / chunkFrameCount;
    return static_cast<int>(std::min<qint64>(chunkCount, MaximumChunksPerEvaluation));
}

void SoundOutput::stop()
{
    if (mSink)
        mSink->stop();
    mDevice = nullptr;
    mSink.reset();
    mPending.clear();
    mTargetBufferedFrameCount = 0;
    mHasAudio = false;
}

void SoundOutput::start(double time)
{
    stop();

    Q_ASSERT(std::isfinite(time) && time >= 0);
    mSampleBase = static_cast<uint64_t>(time * mSampleRate + 0.5);
    mStartSampleBase = mSampleBase;
    if (!mPlaying)
        return;

    const auto device = QMediaDevices::defaultAudioOutput();
    if (device.isNull())
        return;

    auto format = QAudioFormat{ };
    format.setSampleRate(mSampleRate);
    format.setChannelCount(ChannelCount);
    format.setSampleFormat(QAudioFormat::Float);
    if (!device.isFormatSupported(format))
        return;

    mSink = std::make_unique<QAudioSink>(device, format);
    const auto durationFrameCount =
        (mSampleRate * mBufferDurationMilliseconds + 999) / 1000;
    const auto minimumBufferFrameCount =
        std::max(mChunkFrameCount, durationFrameCount);
    const auto targetChunkCount =
        (minimumBufferFrameCount + mChunkFrameCount - 1) / mChunkFrameCount;
    mTargetBufferedFrameCount = targetChunkCount * mChunkFrameCount;
    mSink->setBufferSize(mTargetBufferedFrameCount * BytesPerFrame);
}

void SoundOutput::stream()
{
    if (!mPlaying || !mDevice || mPending.isEmpty())
        return;
    const auto written = mDevice->write(mPending);
    if (written > 0)
        mPending.remove(0, written);
}

std::optional<double> SoundOutput::playbackTime() const
{
    if (!mPlaying || !mHasAudio || !mSink || mSampleRate <= 0)
        return std::nullopt;
    return static_cast<double>(mStartSampleBase) / mSampleRate
        + mSink->processedUSecs() / 1'000'000.0;
}

std::optional<double> SoundOutput::generationTime() const
{
    if (!mPlaying || !mSink || !mHasAudio || mSampleRate <= 0)
        return std::nullopt;
    return static_cast<double>(mSampleBase) / mSampleRate;
}

void SoundOutput::writeFrame(MediaFrame frame)
{
    const auto &samples = frame.audioSamples();
    stream();
    if (!mPlaying || !mSink || samples.isEmpty())
        return;

    if (frame.audioSampleRate() != mSampleRate
        || samples.size() % BytesPerFrame != 0)
        return;

    const auto frameCount = samples.size() / BytesPerFrame;
    if (frameCount <= 0 || frameCount % mChunkFrameCount != 0)
        return;

    mHasAudio = true;
    mPending.append(samples);
    mSampleBase += static_cast<uint64_t>(frameCount);
    if (!mDevice
        && mPending.size() >= mTargetBufferedFrameCount * BytesPerFrame)
        mDevice = mSink->start();

    if (!mDevice) {
        mSink.reset();
        mPending.clear();
        mHasAudio = false;
        return;
    }
    stream();
}

#endif // defined(MULTIMEDIA_ENABLED)
