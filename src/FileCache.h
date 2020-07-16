#ifndef FILECACHE_H
#define FILECACHE_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QMutex>
#include <QTimer>
#include <QFileSystemWatcher>
#include "TextureData.h"

class VideoPlayer;

class FileCache : public QObject
{
    Q_OBJECT
public:
    explicit FileCache(QObject *parent = nullptr);
    ~FileCache();

    bool getSource(const QString &fileName, QString *source) const;
    bool getTexture(const QString &fileName, TextureData *texture) const;
    bool getBinary(const QString &fileName, QByteArray *binary) const;
    bool updateTexture(const QString &fileName, TextureData texture) const;
    void asyncOpenVideoPlayer(const QString &fileName);

    // only call from main thread
    void invalidateEditorFile(const QString &fileName);
    void advertiseEditorSave(const QString &fileName);
    void updateEditorFiles();
    void playVideoFiles();
    void pauseVideoFiles();
    void rewindVideoFiles();

Q_SIGNALS:
    void fileChanged(const QString &fileName) const;
    void videoPlayerRequested(const QString &fileName, QPrivateSignal);

private:
    void handleFileSystemFileChanged(const QString &fileName);
    void addFileSystemWatch(const QString &fileName, bool changed = false) const;
    void updateFileSystemWatches();
    void handleVideoPlayerRequested(const QString &fileName);
    void handleVideoPlayerLoaded();

    QSet<QString> mEditorFilesInvalidated;
    QTimer mUpdateFileSystemWatchesTimer;
    QSet<QString> mEditorSaveAdvertised;
    QFileSystemWatcher mFileSystemWatcher;
    std::map<QString, std::unique_ptr<VideoPlayer>> mVideoPlayers;
    bool mVideosPlaying{ };

    mutable QMutex mMutex;
    mutable QMap<QString, QString> mSources;
    mutable QMap<QString, TextureData> mTextures;
    mutable QMap<QString, QByteArray> mBinaries;
    mutable QMap<QString, bool> mFileSystemWatchesToAdd;
};

#endif // FILECACHE_H
