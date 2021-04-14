#if defined(Qt5Multimedia_FOUND)

#include "VideoPlayer.h"
#include "TextureData.h"
#include "Singletons.h"
#include "FileCache.h"
#include <QMediaPlaylist>
#include <cstring>

VideoPlayer::VideoPlayer(QString fileName, bool flipVertically, QObject *parent)
    : QAbstractVideoSurface(parent)
    , mFileName(fileName)
    , mFlipVertically(flipVertically)
{
    mPlayer = new QMediaPlayer(this);
    connect(mPlayer, &QMediaPlayer::mediaStatusChanged,
        this, &VideoPlayer::handleStatusChanged);
    mPlayer->setVideoOutput(this);
    mPlayer->setMuted(true);

    auto playlist = new QMediaPlaylist(this);
    playlist->setPlaybackMode(QMediaPlaylist::PlaybackMode::CurrentItemInLoop);
    playlist->addMedia(QUrl::fromLocalFile(fileName));
    mPlayer->setPlaylist(playlist);
}

QList<QVideoFrame::PixelFormat> VideoPlayer::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType handleType) const
{
    if (handleType != QAbstractVideoBuffer::NoHandle)
        return { };

    return { 
        QVideoFrame::Format_ARGB32,
        QVideoFrame::Format_BGRA32,
    };
}

void VideoPlayer::handleStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::InvalidMedia) {
        mPlayer->deleteLater();
        Q_EMIT loadingFinished();
    }
    else if (status == QMediaPlayer::LoadedMedia) {
        mPlayer->pause();
    }
}

bool VideoPlayer::present(const QVideoFrame &frame)
{
    if (!mWidth) {
        mWidth = frame.width();
        mHeight = frame.height();
        Q_EMIT loadingFinished();
    }
    auto texture = TextureData();
    if (texture.create(QOpenGLTexture::Target2D,
            QOpenGLTexture::RGBA8_UNorm, frame.width(), frame.height(), 1, 1, 1, 0)) {
        texture.clear();

        if (frame.pixelFormat() == QVideoFrame::Format_ARGB32)
            texture.setPixelFormat(QOpenGLTexture::BGRA);
    
        if (auto buffer = frame.buffer()) {
            auto size = 0;
            auto stride = 0;
            if (auto data = buffer->map(QAbstractVideoBuffer::ReadOnly, &size, &stride)) {
                std::memcpy(texture.getWriteonlyData(0, 0, 0), data, std::min(size, texture.getImageSize(0)));
                buffer->unmap();
                return Singletons::fileCache().updateTexture(
                    mFileName, mFlipVertically, std::move(texture));
            }
        }
    }
    return false;
}

void VideoPlayer::play()
{
    if (mPlayer)
        mPlayer->play();
}

void VideoPlayer::pause()
{
    if (mPlayer)
        mPlayer->pause();
}

void VideoPlayer::rewind()
{
    if (mPlayer)
        mPlayer->setPosition(0);
}

#endif // Qt5Multimedia_FOUND
