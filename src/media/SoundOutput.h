#pragma once

#include <QByteArray>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#if defined(MULTIMEDIA_ENABLED)

class QAudioSink;
class QIODevice;

class SoundOutput
{
public:
    SoundOutput();
    ~SoundOutput();

    void synchronizeToAppTime();
    void setAppTime(double time, bool resetTimeline);
    void setPlaying(bool playing);
    bool resetGenerationRequested();
    int sampleRate() const { return mSampleRate; }
    int bufferFrameCount() const { return mBufferFrameCount; }
    uint64_t sampleBase() const { return mSampleBase; }
    std::optional<double> playbackTime() const;
    std::optional<double> generationTime() const;
    void mix(std::vector<QByteArray> soundBuffers);

private:
    void stop();
    void start(double time);
    void stream();
    bool canAcceptBuffer() const;

    std::unique_ptr<QAudioSink> mSink;
    QIODevice *mDevice{ };
    int mBufferFrameCount{ };
    QByteArray mPending;
    uint64_t mStartSampleBase{ };
    std::atomic<bool> mPlaying{ };
    std::atomic<bool> mGenerationRequested{ };
    int mSampleRate{ 44100 };
    uint64_t mSampleBase{ };
    uint64_t mGenerationSampleBase{ };
    bool mSynchronizeToAppTime{ };
    bool mHasSoundCall{ };
};

#else // !defined(MULTIMEDIA_ENABLED)

class SoundOutput
{
public:
    void synchronizeToAppTime() { }
    void setAppTime(double, bool) { }
    void setPlaying(bool) { }
    bool resetGenerationRequested() { return false; }
    int sampleRate() const { return 44100; }
    int bufferFrameCount() const { return 0; }
    uint64_t sampleBase() const { return 0; }
    std::optional<double> playbackTime() const { return std::nullopt; }
    std::optional<double> generationTime() const { return std::nullopt; }
    void mix(std::vector<QByteArray>) { }
};

#endif // defined(MULTIMEDIA_ENABLED)
