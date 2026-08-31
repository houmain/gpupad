#if defined(MULTIMEDIA_ENABLED)

#  include "SoundOutput.h"
#  include <QAudioDevice>
#  include <QAudioFormat>
#  include <QAudioSink>
#  include <QIODevice>
#  include <QMediaDevices>
#  include <algorithm>
#  include <cmath>
#  include <utility>

namespace {
    inline constexpr auto workGroupSize = uint32_t{ 256 };
    inline constexpr auto maximumBufferFrameCount = uint32_t{ 65536 };

    inline constexpr uint32_t getBufferFrameCount(int sampleRate)
    {
        constexpr auto targetDurationMilliseconds = uint64_t{ 50 };
        constexpr auto alignment = static_cast<uint64_t>(workGroupSize);
        constexpr auto maximum = static_cast<uint64_t>(maximumBufferFrameCount);
        const auto rate = static_cast<uint64_t>(std::max(sampleRate, 1));
        const auto durationFrames = (rate * targetDurationMilliseconds + 999)
            / 1000;
        const auto alignedFrames = (durationFrames + alignment - 1) / alignment
            * alignment;
        return static_cast<uint32_t>(
            std::clamp(alignedFrames, alignment, maximum));
    }

    inline uint64_t getSampleAtTime(double time, int sampleRate)
    {
        if (!std::isfinite(time) || time <= 0.0)
            return 0;
        const auto sample = static_cast<long double>(time)
            * std::max(sampleRate, 1);
        const auto maximum =
            static_cast<long double>(std::numeric_limits<uint64_t>::max()
                - getBufferFrameCount(sampleRate));
        return static_cast<uint64_t>(std::min(sample + 0.5L, maximum));
    }
} // namespace

SoundOutput::SoundOutput() = default;

SoundOutput::~SoundOutput()
{
    stop();
}

void SoundOutput::synchronizeToAppTime()
{
    mSynchronizeToAppTime = true;
    mGenerationRequested = false;
}

void SoundOutput::setAppTime(double appTime, bool reset)
{
    if (reset)
        mHasSoundCall = false;

    if (std::exchange(mSynchronizeToAppTime, false) || reset)
        start(appTime);

    mGenerationSampleBase = mSampleBase;
    if (mPlaying.load() && mGenerationRequested.load())
        mGenerationSampleBase += getBufferFrameCount(sampleRate());
}

void SoundOutput::setPlaying(bool playing)
{
    const auto wasPlaying = mPlaying.exchange(playing);
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
    mGenerationRequested = canAcceptBuffer();
}

bool SoundOutput::resetGenerationRequested()
{
    return mGenerationRequested.exchange(false);
}

void SoundOutput::stop()
{
    if (mSink)
        mSink->stop();
    mDevice = nullptr;
    mSink.reset();
    mBufferFrameCount = 0;
    mPending.clear();
}

void SoundOutput::start(double time)
{
    stop();

    const auto device = QMediaDevices::defaultAudioOutput();
    if (device.isNull())
        return;

    const auto rate = device.preferredFormat().sampleRate();
    if (rate <= 0)
        return;
    mSampleRate = rate;
    mBufferFrameCount = getBufferFrameCount(rate);

    auto format = QAudioFormat{ };
    format.setSampleRate(rate);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);
    if (!device.isFormatSupported(format))
        return;

    mSink = std::make_unique<QAudioSink>(device, format);
    mSink->setBufferSize(mBufferFrameCount * format.bytesPerFrame());
    mDevice = mSink->start();
    if (!mDevice) {
        mSink.reset();
        return;
    }
    if (!mPlaying.load())
        mSink->suspend();

    mSampleBase = getSampleAtTime(time, sampleRate());
    mStartSampleBase = mSampleBase;
    mGenerationSampleBase = mSampleBase;
    mGenerationRequested = canAcceptBuffer();
}

void SoundOutput::stream()
{
    if (!mPlaying.load() || !mDevice || mPending.isEmpty())
        return;
    const auto written = mDevice->write(mPending);
    if (written > 0)
        mPending.remove(0, written);
}

bool SoundOutput::canAcceptBuffer() const
{
    return mPlaying.load() && mDevice && mPending.isEmpty();
}

std::optional<double> SoundOutput::playbackTime() const
{
    if (!mPlaying.load() || !mHasSoundCall)
        return std::nullopt;
    if (!mSink || mSampleRate <= 0)
        return std::nullopt;
    return static_cast<double>(mStartSampleBase) / mSampleRate
        + static_cast<double>(mSink->processedUSecs()) / 1'000'000.0;
}

std::optional<double> SoundOutput::generationTime() const
{
    const auto rate = sampleRate();
    if (!mPlaying.load() || !mDevice || rate <= 0)
        return std::nullopt;
    return static_cast<double>(mGenerationSampleBase) / rate;
}

void SoundOutput::mix(std::vector<QByteArray> soundBuffers)
{
    if (!soundBuffers.empty())
        mHasSoundCall = true;

    stream();
    if (!mHasSoundCall)
        return;
    if (soundBuffers.empty()) {
        if (canAcceptBuffer())
            mGenerationRequested = true;
        return;
    }
    if (!canAcceptBuffer())
        return;

    const auto bufferSize = soundBuffers.front().size();
    const auto frameCount = bufferSize / (2 * static_cast<int>(sizeof(float)));
    Q_ASSERT(frameCount == mBufferFrameCount);
    if (frameCount != mBufferFrameCount)
        return;
    for (const auto &soundBuffer : soundBuffers) {
        Q_ASSERT(soundBuffer.size() == bufferSize);
        if (soundBuffer.size() != bufferSize)
            return;
    }

    mPending = std::move(soundBuffers.front());
    const auto floatCount = mPending.size() / static_cast<int>(sizeof(float));
    auto *values = reinterpret_cast<float *>(mPending.data());
    for (auto i = size_t{ 1 }; i < soundBuffers.size(); ++i) {
        const auto &soundBuffer = soundBuffers[i];
        const auto *source =
            reinterpret_cast<const float *>(soundBuffer.constData());
        for (auto j = 0; j < floatCount; ++j)
            if (std::isfinite(source[j]))
                values[j] += source[j];
    }
    for (auto i = 0; i < floatCount; ++i)
        values[i] = (std::isfinite(values[i])
                ? std::clamp(values[i], -1.0f, 1.0f)
                : 0.0f);
    stream();
    mSampleBase += static_cast<uint64_t>(frameCount);
    if (canAcceptBuffer())
        mGenerationRequested = true;
}

#endif // defined(MULTIMEDIA_ENABLED)
