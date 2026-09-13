#include "MediaStream.h"

#if defined(MULTIMEDIA_ENABLED)

#  include "FileCache.h"
#  include "Singletons.h"
#  include "TextureData.h"
#  include <utility>

MediaStream::MediaStream(MediaSource source, QObject *parent)
    : QObject(parent)
    , mSource(std::move(source))
{
}

void MediaStream::presentFrame(const QVideoFrame &frame)
{
    Q_ASSERT(onMainThread());
    setSize(frame.width(), frame.height());
    if (mCurrentFrame == frame)
        return;
    mCurrentFrame = frame;
    Singletons::fileCache().updateMediaTexture(mSource, frame);
}

void MediaStream::presentTexture(TextureData texture)
{
    Q_ASSERT(onMainThread());
    Q_ASSERT(!texture.isNull());
    setSize(texture.width(), texture.height());
    Singletons::fileCache().updateMediaTexture(mSource, std::move(texture));
}

void MediaStream::finishLoading()
{
    setReady(true);
}

void MediaStream::setReady(bool ready)
{
    if (!ready) {
        mWidth = 0;
        mHeight = 0;
        mCurrentFrame = { };
    }
    if (std::exchange(mReady, ready) != ready)
        Q_EMIT readyChanged();
}

void MediaStream::setSize(int width, int height)
{
    if (!mWidth) {
        mWidth = width;
        mHeight = height;
    }
    finishLoading();
}

#endif // defined(MULTIMEDIA_ENABLED)
