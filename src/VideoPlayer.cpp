#if defined(QtMultimedia_FOUND)

#  include "VideoPlayer.h"
#  include "FileCache.h"
#  include "Singletons.h"
#  include "TextureData.h"
#  include <QVideoFrame>
#  include <cstring>

VideoPlayer::VideoPlayer(QString fileName, bool flipVertically, QObject *parent)
    : QVideoSink(parent)
    , mFileName(fileName)
    , mFlipVertically(flipVertically)
{
    mPlayer = new QMediaPlayer(this);
    connect(mPlayer, &QMediaPlayer::mediaStatusChanged, this,
        &VideoPlayer::handleStatusChanged);
    connect(this, &QVideoSink::videoFrameChanged, this,
        &VideoPlayer::handleVideoFrame);
    mPlayer->setSource(QUrl::fromLocalFile(fileName));
    mPlayer->setLoops(QMediaPlayer::Infinite);
    mPlayer->setVideoSink(this);
}

void VideoPlayer::handleStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::InvalidMedia) {
        mPlayer->deleteLater();
        mPlayer = nullptr;
        Q_EMIT loadingFinished();
    } else if (status == QMediaPlayer::LoadedMedia) {
        mPlayer->pause();
    } else if (status == QMediaPlayer::EndOfMedia) {
        mPlayer->play();
    }
}

void VideoPlayer::handleVideoFrame(const QVideoFrame &frame)
{
    if (!mWidth) {
        mWidth = frame.width();
        mHeight = frame.height();
        Q_EMIT loadingFinished();
    }
    auto texture = TextureData();
    if (texture.loadQImage(frame.toImage(), mFlipVertically))
        Singletons::fileCache().updateTexture(mFileName, mFlipVertically,
            std::move(texture));
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

#endif // QtMultimedia_FOUND
