
#include "VideoManager.h"
#include "FileCache.h"
#include "Singletons.h"
#include "VideoPlayer.h"

VideoManager::VideoManager(QObject *parent) : QObject(parent) { }

VideoManager::~VideoManager() = default;

void VideoManager::unloadAll()
{
    Q_ASSERT(onMainThread());
    for (const auto &[fileName, videoPlayer] : mVideoPlayers)
        Singletons::fileCache().invalidateFile(fileName);
    mVideoPlayers.clear();
}

void VideoManager::handleVideoPlayerRequested(const QString &fileName,
    bool flipVertically)
{
    Q_ASSERT(onMainThread());
    auto videoPlayer = new VideoPlayer(fileName, flipVertically);
    connect(videoPlayer, &VideoPlayer::loadingFinished, this,
        &VideoManager::handleVideoPlayerLoaded);
}

void VideoManager::handleVideoPlayerLoaded()
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

void VideoManager::seek(double time)
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
