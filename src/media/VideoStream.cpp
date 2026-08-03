
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

void VideoStream::presentFrame(const QVideoFrame &frame)
{
    Q_ASSERT(onMainThread());
    if (!mWidth) {
        mWidth = frame.width();
        mHeight = frame.height();
        Q_EMIT loadingFinished();
    }
    if (mCurrentFrame == frame)
        return;
    mCurrentFrame = frame;
    Singletons::fileCache().updateVideoTexture(mFileName, mFlipVertically,
        frame);
}

#endif // defined(MULTIMEDIA_ENABLED)
