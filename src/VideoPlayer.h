#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#if defined(Qt5Multimedia_FOUND)

#include <QAbstractVideoSurface>
#include <QMediaPlayer>

class VideoPlayer : public QAbstractVideoSurface
{
    Q_OBJECT
public:
    explicit VideoPlayer(QString fileName, QObject *parent = nullptr);

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
        QAbstractVideoBuffer::HandleType) const override;
    bool present(const QVideoFrame &frame) override;

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

    QMediaPlayer *mPlayer{ };
    QString mFileName;
    int mWidth{ };
    int mHeight{ };
};

#else // !Qt5Multimedia_FOUND

#include <QObject>

class VideoPlayer : public QObject
{
    Q_OBJECT
public:
    explicit VideoPlayer(QString fileName, QObject *parent = nullptr)
        : QObject(parent), mFileName(fileName) { }
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

#endif // !Qt5Multimedia_FOUND

#endif // VIDEOPLAYER_H
