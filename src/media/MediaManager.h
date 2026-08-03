#pragma once

#include <QObject>
#include <map>
#include <memory>

class VideoPlayer;

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

    void handleVideoPlayerRequested(const QString &fileName,
        bool flipVertically);

private:
    void handleVideoPlayerLoaded();

    std::map<QString, std::unique_ptr<VideoPlayer>> mVideoPlayers;
    std::chrono::milliseconds mTargetTime{};
};
