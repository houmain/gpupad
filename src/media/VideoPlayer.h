#pragma once

#if defined(MULTIMEDIA_ENABLED)

#  include "MediaStream.h"
#  include <QMediaPlayer>

class QVideoSink;

class VideoPlayer final : public MediaStream
{
public:
    VideoPlayer(MediaSource source, QObject *parent = nullptr);

    void seek(std::chrono::milliseconds time) override;

private:
    void handleStatusChanged(QMediaPlayer::MediaStatus status);
    void handleFrameDecoded(QVideoFrame frame);
    void updateTargetTime();

    QMediaPlayer *mPlayer{ };
    QVideoSink *mSink{ };
    double mPlaybackSpeed{ 1.0 };
    std::vector<QVideoFrame> mFrameQueue;
    std::chrono::milliseconds mTargetTime{ };
    std::chrono::microseconds mDecodeTime{ };
    std::chrono::microseconds mDuration{ };
    int mLoopCount{ };
    bool mSeeking{ };
};

#endif // !MULTIMEDIA_ENABLED
