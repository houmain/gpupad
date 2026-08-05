#pragma once

#include <QObject>
#include <map>
#include <chrono>
#include <memory>

class VideoStream;

class MediaManager : public QObject
{
    Q_OBJECT
public:
    explicit MediaManager(QObject *parent = nullptr);
    MediaManager(const MediaManager &) = delete;
    MediaManager &operator=(const MediaManager &) = delete;
    ~MediaManager();

    void unloadAll();
    void seek(double time);
    void pause();
    void handleMediaRequested(const QString &fileName, bool flipVertically);

private:
    void handleMediaLoaded();
    void seekToTargetTime();

    std::map<QString, std::unique_ptr<VideoStream>> mVideoStreams;
    std::chrono::milliseconds mTargetTime{ };
};
