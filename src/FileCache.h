#pragma once

#include "media/MediaSource.h"
#include "TextureData.h"
#include <QFileSystemWatcher>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <map>

class QVideoFrame;

class FileCache final : public QObject
{
    Q_OBJECT
public:
    explicit FileCache(QObject *parent = nullptr);
    ~FileCache();

    bool getSource(const QString &fileName, QString *source) const;
    bool getTexture(const QString &fileName, TextureData *texture) const;
    bool getTexture(const MediaSource &source, TextureData *texture) const;
    bool getBinary(const QString &fileName, QByteArray *binary) const;

    void updateSource(const QString &fileName, QString source);
    void updateTexture(const QString &fileName, TextureData texture);
    void updateMediaTexture(const MediaSource &source,
        const QVideoFrame &frame);
    void updateMediaTexture(const MediaSource &source, TextureData texture);
    void updateBinary(const QString &fileName, QByteArray binary);
    void updateBinaryRange(const QString &fileName, int offset,
        const QByteArray &range);

    // only call from main thread
    void unloadAll();
    void invalidateFile(const QString &fileName);
    void invalidateMedia(const MediaSource &source);
    void handleEditorFileChanged(const QString &fileName,
        bool emitFileChanged = true);
    void handleEditorSave(const QString &fileName);
    void updateFromEditors();

Q_SIGNALS:
    void fileChanged(const QString &fileName);
    void mediaRequested(MediaSource source) const;
    void reloadSource(const QString &fileName, QPrivateSignal);
    void reloadTexture(const QString &fileName, QPrivateSignal);
    void reloadBinary(const QString &fileName, QPrivateSignal);
    void convertVideoFrame(MediaSource source, const QVideoFrame &frame,
        QPrivateSignal);

public Q_SLOTS:
    void handleSourceReloaded(const QString &fileName, QString);
    void handleTextureReloaded(const QString &fileName, TextureData);
    void handleMediaTextureLoaded(MediaSource source, TextureData texture);
    void handleBinaryReloaded(const QString &fileName, QByteArray);
    void handleReloadingFailed(const QString &fileName);

private:
    class BackgroundLoader;

    void handleFileSystemFileChanged(const QString &fileName);
    void addFileSystemWatch(const QString &fileName,
        bool changed = false) const;
    void updateFileSystemWatches();
    bool reloadFileInBackground(const QString &fileName);
    bool updateFromEditor(const QString &fileName);
    void purgeFile(const QString &fileName);

    mutable QMutex mMutex;
    mutable QMap<QString, QString> mSources;
    mutable std::map<MediaSource, TextureData> mTextures;
    mutable QMap<QString, QByteArray> mBinaries;
    mutable QMap<QString, bool> mFileSystemWatchesToAdd;

    QSet<QString> mEditorFilesChanged;
    QSet<QString> mEditorSaveAdvertised;
    QTimer mUpdateFileSystemWatchesTimer;
    QFileSystemWatcher mFileSystemWatcher;
    int mFileSystemWatcherUpdate{ };
    QThread mBackgroundLoaderThread;
};
