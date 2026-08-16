#pragma once

#if defined(MULTIMEDIA_ENABLED)

#  include "VideoStream.h"
#  include <QMediaPlayer>

class QVideoSink;

class VideoPlayer final : public VideoStream
{
public:
    VideoPlayer(QString fileName, QObject *parent = nullptr);

    void seek(std::chrono::milliseconds time) override;

private:
    void handleStatusChanged(QMediaPlayer::MediaStatus status);
    void handleFrameDecoded(QVideoFrame frame);

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
