
#include "MediaManager.h"
#include "FileCache.h"
#include "Singletons.h"
#include "VideoPlayer.h"

MediaManager::MediaManager(QObject *parent) : QObject(parent) { }

MediaManager::~MediaManager() = default;

void MediaManager::unloadAll()
{
    Q_ASSERT(onMainThread());
    for (const auto &[fileName, videoPlayer] : mVideoPlayers)
        Singletons::fileCache().invalidateFile(fileName);
    mVideoPlayers.clear();
}

void MediaManager::handleVideoPlayerRequested(const QString &fileName,
    bool flipVertically)
{
    Q_ASSERT(onMainThread());
    auto videoPlayer = new VideoPlayer(fileName, flipVertically);
    connect(videoPlayer, &VideoPlayer::loadingFinished, this,
        &MediaManager::handleVideoPlayerLoaded);
}

void MediaManager::handleVideoPlayerLoaded()
{
    Q_ASSERT(onMainThread());
    auto videoPlayer = qobject_cast<VideoPlayer *>(QObject::sender());
    if (videoPlayer->width()) {
        videoPlayer->seek(mTargetTime);
        mVideoPlayers[videoPlayer->fileName()].reset(videoPlayer);
    } else {
        videoPlayer->deleteLater();
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

    for (const auto &videoPlayer : mVideoPlayers)
        videoPlayer.second->seek(targetTime);
    mTargetTime = targetTime;
}
