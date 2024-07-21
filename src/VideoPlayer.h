#pragma once

#if defined(Qt6Multimedia_FOUND)

#  include <QMediaPlayer>
#  include <QVideoSink>

class VideoPlayer final : public QVideoSink
{
    Q_OBJECT
public:
    VideoPlayer(QString fileName, bool flipVertically,
        QObject *parent = nullptr);

    const QString &fileName() const { return mFileName; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    void play();
    void pause();
    void rewind();

Q_SIGNALS:
    void loadingFinished();

private:
    void handleStatusChanged(QMediaPlayer::MediaStatus status);
    void handleVideoFrame(const QVideoFrame &frame);

    QMediaPlayer *mPlayer{};
    QString mFileName;
    int mWidth{};
    int mHeight{};
    bool mFlipVertically{};
};

#else // !Qt6Multimedia_FOUND

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
    void play() { }
    void pause() { }
    void rewind() { }

Q_SIGNALS:
    void loadingFinished();

private:
    QString mFileName;
};

#endif // !Qt6Multimedia_FOUND
