#pragma once

#include "MediaFrame.h"
#include <QObject>
#include <cstdint>
#include <memory>
#include <optional>

#if defined(MULTIMEDIA_ENABLED)

class QAudioSink;
class QIODevice;

class SoundOutput : public QObject
{
public:
    explicit SoundOutput(QObject *parent = nullptr);
    ~SoundOutput();

    void synchronizeToAppTime();
    void setBufferDuration(int milliseconds);
    void setFormat(int sampleRate, int chunkFrameCount);
    void setAppTime(double time, bool resetTimeline);
    void setPlaying(bool playing);
    int requestedChunkCount(bool hasAudio);
    uint64_t sampleBase() const { return mSampleBase; }
    std::optional<double> playbackTime() const;
    std::optional<double> generationTime() const;
    void writeFrame(MediaFrame frame);

private:
    void stop();
    void start(double time);
    void stream();

    std::unique_ptr<QAudioSink> mSink;
    QIODevice *mDevice{ };
    QByteArray mPending;
    int mTargetBufferedFrameCount{ };
    uint64_t mStartSampleBase{ };
    bool mPlaying{ };
    int mSampleRate{ };
    int mChunkFrameCount{ };
    int mBufferDurationMilliseconds{ 250 };
    uint64_t mSampleBase{ };
    bool mSynchronizeToAppTime{ };
    bool mHasAudio{ };
};

#else // !defined(MULTIMEDIA_ENABLED)

class SoundOutput : public QObject
{
public:
    explicit SoundOutput(QObject *parent = nullptr) : QObject(parent) { }
    void synchronizeToAppTime() { }
    void setBufferDuration(int) { }
    void setFormat(int, int) { }
    void setAppTime(double, bool) { }
    void setPlaying(bool) { }
    int requestedChunkCount(bool) { return 0; }
    uint64_t sampleBase() const { return 0; }
    std::optional<double> playbackTime() const { return std::nullopt; }
    std::optional<double> generationTime() const { return std::nullopt; }
    void writeFrame(MediaFrame) { }
};

#endif // defined(MULTIMEDIA_ENABLED)
