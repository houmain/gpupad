#pragma once

#include <QObject>
#include <map>

class VideoPlayer;

class VideoManager : public QObject
{
    Q_OBJECT
public:
    explicit VideoManager(QObject *parent = nullptr);

    void playVideoFiles();
    void pauseVideoFiles();
    void rewindVideoFiles();

    void handleVideoPlayerRequested(const QString &fileName, bool flipVertically);

private:    
    void handleVideoPlayerLoaded();

    std::map<QString, std::unique_ptr<VideoPlayer>> mVideoPlayers;
    bool mVideosPlaying{ };
};
