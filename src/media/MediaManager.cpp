
#include "MediaManager.h"
#include "FileCache.h"
#include "Singletons.h"
#include "VideoPlayer.h"
#include "Camera.h"
#include "AudioSpectrum.h"
#include "FileDialog.h"

MediaManager::MediaManager(QObject *parent) : QObject(parent) { }

MediaManager::~MediaManager() = default;

#if defined(MULTIMEDIA_ENABLED)

void MediaManager::unloadAll()
{
    Q_ASSERT(onMainThread());
    mVideoStreams.clear();
}

void MediaManager::unloadFile(const QString &fileName)
{
    Q_ASSERT(onMainThread());
    mVideoStreams.erase(fileName);
}

void MediaManager::unloadFiles(std::function<bool(const QString &)> predicate)
{
    for (auto it = mVideoStreams.begin(); it != mVideoStreams.end();)
        if (predicate(it->first)) {
            it = mVideoStreams.erase(it);
        } else {
            ++it;
        }
}

void MediaManager::handleMediaRequested(const QString &fileName,
    QSize resolution)
{
    Q_ASSERT(onMainThread());
    auto videoStream = std::add_pointer_t<VideoStream>{ };
    if (FileDialog::isAudioFileName(fileName)) {
        videoStream = new AudioSpectrum(fileName, resolution);
    } else if (FileDialog::isCameraFileName(fileName)) {
        videoStream = new Camera(fileName);
    } else {
        videoStream = new VideoPlayer(fileName);
    }
    connect(videoStream, &VideoStream::loadingFinished, this,
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

void MediaManager::seekToTargetTime()
{
    for (const auto &[fileName, videoStream] : mVideoStreams)
        videoStream->seek(mTargetTime);
}

void MediaManager::seek(double time)
{
    Q_ASSERT(onMainThread());
    const auto targetTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(time));
    if (mTargetTime == targetTime)
        return;
    mTargetTime = targetTime;
    seekToTargetTime();
}

void MediaManager::pause()
{
    Q_ASSERT(onMainThread());
    seekToTargetTime();
}

#else // !defined(MULTIMEDIA_ENABLED)

class VideoStream
{
};

void MediaManager::unloadAll() { }
void MediaManager::unloadFile(const QString &fileName) { }
void MediaManager::unloadFiles(std::function<bool(const QString &)> predicate)
{
}
void MediaManager::handleMediaRequested(const QString &, QSize resolution) { }
void MediaManager::seek(double time) { }
void MediaManager::pause() { }

#endif // !defined(MULTIMEDIA_ENABLED)
