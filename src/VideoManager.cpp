
#include "VideoManager.h"
#include "FileCache.h"
#include "Singletons.h"
#include "VideoPlayer.h"

VideoManager::VideoManager(QObject *parent) : QObject(parent) { }

VideoManager::~VideoManager() = default;

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
        if (mVideosPlaying)
            videoPlayer->play();
        mVideoPlayers[videoPlayer->fileName()].reset(videoPlayer);
    } else {
        videoPlayer->deleteLater();
    }
}

void VideoManager::playVideoFiles()
{
    Q_ASSERT(onMainThread());
    for (const auto &videoPlayer : mVideoPlayers)
        videoPlayer.second->play();
    mVideosPlaying = true;
}

void VideoManager::pauseVideoFiles()
{
    Q_ASSERT(onMainThread());
    for (const auto &videoPlayer : mVideoPlayers)
        videoPlayer.second->pause();
    mVideosPlaying = false;
}

void VideoManager::rewindVideoFiles()
{
    Q_ASSERT(onMainThread());
    for (const auto &videoPlayer : mVideoPlayers)
        videoPlayer.second->rewind();
}
