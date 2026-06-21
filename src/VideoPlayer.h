#pragma once

#if defined(VIDEOPLAYER_ENABLED)

#  include <QMediaPlayer>
#  include <QVideoSink>
#  include <QVideoFrame>

class VideoPlayer final : public QVideoSink
{
    Q_OBJECT
public:
    VideoPlayer(QString fileName, bool flipVertically,
        QObject *parent = nullptr);

    const QString &fileName() const { return mFileName; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    void seek(std::chrono::milliseconds targetTime);

Q_SIGNALS:
    void loadingFinished();

private:
    void handleStatusChanged(QMediaPlayer::MediaStatus status);
    void handleFrameDecoded(QVideoFrame frame);
    void presentFrame(const QVideoFrame &frame);

    QMediaPlayer *mPlayer{};
    QString mFileName;
    int mWidth{};
    int mHeight{};
    bool mFlipVertically{};
    double mPlaybackSpeed{ 1.0 };
    std::vector<QVideoFrame> mFrameQueue;
    QVideoFrame mCurrentFrame;
    std::chrono::milliseconds mTargetTime{};
    std::chrono::microseconds mDecodeTime{};
    std::chrono::microseconds mDuration{};
    int mLoopCount{};
    bool mSeeking{};
};

#else // !VIDEOPLAYER_ENABLED

#  include <QObject>

class VideoPlayer final : public QObject
{
    Q_OBJECT
public:
    explicit VideoPlayer(QString fileName, bool flipVertically,
        QObject *parent = nullptr)
        : QObject(parent)
        , mFileName(fileName)
    {
    }
    const QString &fileName() const { return mFileName; }
    int width() const { return 0; }
    int height() const { return 0; }
    void seek(std::chrono::milliseconds time) { }

Q_SIGNALS:
    void loadingFinished();

private:
    QString mFileName;
};

#endif // !VIDEOPLAYER_ENABLED
