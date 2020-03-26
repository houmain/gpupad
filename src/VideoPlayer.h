#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

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

signals:
    void loadingFinished();

private:
    void handleStatusChanged(QMediaPlayer::MediaStatus status);

    QMediaPlayer *mPlayer{ };
    QString mFileName;
    int mWidth{ };
    int mHeight{ };
};

#endif // VIDEOPLAYER_H
