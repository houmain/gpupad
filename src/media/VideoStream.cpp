
#include "VideoStream.h"
#include "Singletons.h"
#include "FileCache.h"

#if defined(MULTIMEDIA_ENABLED)

VideoStream::VideoStream(QString fileName, bool flipVertically, QObject *parent)
    : QObject(parent)
    , mFileName(fileName)
    , mFlipVertically(flipVertically)
{
}

void VideoStream::finishedLoading(int width, int height)
{
    mWidth = width;
    mHeight = height;

    Q_EMIT loadingFinished();
}

void VideoStream::presentFrame(const QVideoFrame &frame)
{
    if (mCurrentFrame == frame)
        return;
    mCurrentFrame = frame;
    Singletons::fileCache().updateVideoTexture(mFileName, mFlipVertically,
        frame);
}

#endif // defined(MULTIMEDIA_ENABLED)
