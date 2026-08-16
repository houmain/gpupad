
#if defined(MULTIMEDIA_ENABLED)

#  include "VideoStream.h"
#  include "Singletons.h"
#  include "FileCache.h"
#  include "TextureData.h"

VideoStream::VideoStream(QString fileName, QObject *parent)
    : QObject(parent)
    , mFileName(fileName)
{
}

void VideoStream::presentFrame(const QVideoFrame &frame)
{
    Q_ASSERT(onMainThread());
    setSize(frame.width(), frame.height());
    if (mCurrentFrame == frame)
        return;
    mCurrentFrame = frame;
    Singletons::fileCache().updateVideoTexture(mFileName, frame);
}

void VideoStream::presentTexture(TextureData texture)
{
    Q_ASSERT(onMainThread());
    Q_ASSERT(!texture.isNull());
    setSize(texture.width(), texture.height());
    Singletons::fileCache().updateVideoTexture(mFileName, std::move(texture));
}

void VideoStream::setSize(int width, int height)
{
    if (mWidth)
        return;
    mWidth = width;
    mHeight = height;
    Q_EMIT loadingFinished();
}

#endif // defined(MULTIMEDIA_ENABLED)
