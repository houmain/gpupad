
#include "MediaManager.h"
#include "FileCache.h"
#include "Singletons.h"
#include "VideoPlayer.h"
#include "Camera.h"

MediaManager::MediaManager(QObject *parent) : QObject(parent) { }

MediaManager::~MediaManager() = default;

#if defined(MULTIMEDIA_ENABLED)

void MediaManager::unloadAll()
{
    Q_ASSERT(onMainThread());
    for (const auto &[fileName, videoPlayer] : mVideoStreams)
        Singletons::fileCache().invalidateFile(fileName);
    mVideoStreams.clear();
}

void MediaManager::handleMediaRequested(const QString &fileName,
    bool flipVertically)
{
    Q_ASSERT(onMainThread());
    auto videoPlayer = new VideoPlayer(fileName, flipVertically);
    connect(videoPlayer, &VideoPlayer::loadingFinished, this,
        &MediaManager::handleMediaLoaded);
}

void MediaManager::handleMediaLoaded()
{
    Q_ASSERT(onMainThread());
    auto videoStream = qobject_cast<VideoStream *>(QObject::sender());
    Singletons::fileCache().invalidateFile(videoStream->fileName());
    if (videoStream->width()) {
        videoStream->seek(mTargetTime);
        mVideoStreams[videoStream->fileName()].reset(videoStream);
    } else {
        videoStream->deleteLater();
    }
}

void MediaManager::seek(double time)
{
    Q_ASSERT(onMainThread());
    const auto targetTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(time));
    if (mTargetTime == targetTime)
        return;

    for (const auto &[fileName, videoStream] : mVideoStreams)
        videoStream->seek(targetTime);
    mTargetTime = targetTime;
}

#else // !defined(MULTIMEDIA_ENABLED)

void MediaManager::unloadAll() { }
void MediaManager::handleMediaRequested(const QString &, bool) { }
void MediaManager::seek(double time) { }

#endif // !defined(MULTIMEDIA_ENABLED)
