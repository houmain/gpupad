#ifndef FILECACHE_H
#define FILECACHE_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QMutex>
#include <QTimer>
#include <QThread>
#include <QFileSystemWatcher>
#include "TextureData.h"

class FileCache final : public QObject
{
    Q_OBJECT
public:
    explicit FileCache(QObject *parent = nullptr);
    ~FileCache();

    bool getSource(const QString &fileName, QString *source) const;
    bool getTexture(const QString &fileName, bool flipVertically, TextureData *texture) const;
    bool getBinary(const QString &fileName, QByteArray *binary) const;
    bool updateTexture(const QString &fileName, bool flippedVertically, TextureData texture);

    // only call from main thread
    void invalidateFile(const QString &fileName);
    void handleEditorFileChanged(const QString &fileName, bool emitFileChanged = true);
    void handleEditorSave(const QString &fileName);
    void updateFromEditors();

Q_SIGNALS:
    void fileChanged(const QString &fileName);
    void videoPlayerRequested(const QString &fileName, bool flipVertically) const;
    void reloadSource(const QString &fileName, QPrivateSignal);
    void reloadTexture(const QString &fileName, bool flipVertically, QPrivateSignal);
    void reloadBinary(const QString &fileName, QPrivateSignal);

public Q_SLOTS:
    void handleSourceReloaded(const QString &fileName, QString);
    void handleTextureReloaded(const QString &fileName, bool flipVertically, TextureData);
    void handleBinaryReloaded(const QString &fileName, QByteArray);

private:
    class BackgroundLoader;
    using TextureKey = QPair<QString, bool>;

    void handleFileSystemFileChanged(const QString &fileName);
    void addFileSystemWatch(const QString &fileName, bool changed = false) const;
    void updateFileSystemWatches();
    bool reloadFileInBackground(const QString &fileName);
    bool updateFromEditor(const QString &fileName);
    void purgeFile(const QString &fileName);

    mutable QMutex mMutex;
    mutable QMap<QString, QString> mSources;
    mutable QMap<TextureKey, TextureData> mTextures;
    mutable QMap<QString, QByteArray> mBinaries;
    mutable QMap<QString, bool> mFileSystemWatchesToAdd;

    QSet<QString> mEditorFilesChanged;
    QSet<QString> mEditorSaveAdvertised;
    QTimer mUpdateFileSystemWatchesTimer;
    QFileSystemWatcher mFileSystemWatcher;
    int mFileSystemWatcherUpdate{ };
    QThread mBackgroundLoaderThread;
};

#endif // FILECACHE_H
