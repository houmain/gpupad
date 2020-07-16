#if defined(Qt5Multimedia_FOUND)

#include "VideoPlayer.h"
#include "TextureData.h"
#include "Singletons.h"
#include "FileCache.h"
#include <QMediaPlaylist>
#include <cstring>

VideoPlayer::VideoPlayer(QString fileName, QObject *parent)
    : QAbstractVideoSurface(parent)
    , mFileName(fileName)
{
    mPlayer = new QMediaPlayer(this);
    connect(mPlayer, &QMediaPlayer::mediaStatusChanged,
        this, &VideoPlayer::handleStatusChanged);
    mPlayer->setVideoOutput(this);
    mPlayer->setMuted(true);

    auto playlist = new QMediaPlaylist(this);
    playlist->setPlaybackMode(QMediaPlaylist::PlaybackMode::Loop);
    playlist->addMedia(QUrl::fromLocalFile(fileName));
    mPlayer->setPlaylist(playlist);
}

QList<QVideoFrame::PixelFormat> VideoPlayer::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType) const
{
    return { QVideoFrame::Format_ABGR32 };
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
    auto mappableFrame = frame;
    auto texture = TextureData();
    if (texture.create(QOpenGLTexture::Target2D,
          QOpenGLTexture::RGBA8_UNorm, frame.width(), frame.height(), 1, 1, 1))
        if (mappableFrame.map(QAbstractVideoBuffer::ReadOnly))
            if (const auto size = texture.getLevelSize(0); size <= frame.mappedBytes()) {
                std::memcpy(texture.getWriteonlyData(0, 0, 0), mappableFrame.bits(), size);
                return Singletons::fileCache().updateTexture(mFileName, std::move(texture));
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
