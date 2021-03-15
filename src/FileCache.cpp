#include "FileCache.h"
#include "Singletons.h"
#include "VideoPlayer.h"
#include "editors/EditorManager.h"
#include "editors/SourceEditor.h"
#include "editors/TextureEditor.h"
#include "editors/BinaryEditor.h"
#include <QThread>
#include <QTextStream>

FileCache::FileCache(QObject *parent) : QObject(parent)
{
    connect(&mFileSystemWatcher, &QFileSystemWatcher::fileChanged,
        this, &FileCache::handleFileSystemFileChanged);
    connect(&mUpdateFileSystemWatchesTimer, &QTimer::timeout,
        this, &FileCache::updateFileSystemWatches);
    connect(this, &FileCache::videoPlayerRequested,
        this, &FileCache::handleVideoPlayerRequested, Qt::QueuedConnection);

    updateFileSystemWatches();
}

FileCache::~FileCache() = default;

void FileCache::advertiseEditorSave(const QString &fileName)
{
    Q_ASSERT(onMainThread());
    QMutexLocker lock(&mMutex);
    mEditorSaveAdvertised.insert(fileName);
}

void FileCache::invalidateEditorFile(const QString &fileName, bool emitFileChanged)
{
    Q_ASSERT(onMainThread());
    mEditorFilesInvalidated.insert(fileName);

    if (emitFileChanged)
        Q_EMIT fileChanged(fileName);
}

void FileCache::updateEditorFiles()
{
    Q_ASSERT(onMainThread());
    QMutexLocker lock(&mMutex);
    auto &editorManager = Singletons::editorManager();
    for (const auto &fileName : qAsConst(mEditorFilesInvalidated)) {
        if (auto editor = editorManager.getSourceEditor(fileName)) {
            mSources[fileName] = editor->source();
        }
        else if (auto editor = editorManager.getBinaryEditor(fileName)) {
            mBinaries[fileName] = editor->data();
        }
        else if (auto editor = editorManager.getTextureEditor(fileName)) {
            mTextures[fileName] = editor->texture();
        }
        else {
            mSources.remove(fileName);
            mBinaries.remove(fileName);
            mTextures.remove(fileName);
        }
    }
    mEditorFilesInvalidated.clear();
}

bool FileCache::loadSource(const QString &fileName, QString *source) const
{
    if (!source)
        return false;

    if (FileDialog::isEmptyOrUntitled(fileName)) {
        *source = "";
        return true;
    }
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text))
        return false;

    const auto unprintable = [](const auto &string) {
      return (std::find_if(string.constBegin(), string.constEnd(),
          [](QChar c) { return (!c.isPrint() && !c.isSpace()); }) != string.constEnd());
    };

    QTextStream stream(&file);
    auto string = stream.readAll();
    if (!string.isSimpleText() || unprintable(string)) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
      stream.setCodec("Windows-1250");
      stream.seek(0);
      string = stream.readAll();
#endif
      if (unprintable(string))
          return false;
    }

    *source = string;
    return true;
}

bool FileCache::loadTexture(const QString &fileName, TextureData *texture) const
{
    if (!texture || FileDialog::isEmptyOrUntitled(fileName))
        return false;

    auto file = TextureData();
    if (!file.load(fileName)) {
        if (FileDialog::isVideoFileName(fileName)) {
            texture->create(QOpenGLTexture::Target2D,
                QOpenGLTexture::RGBA8_UNorm, 1, 1, 1, 1, 1);
            asyncOpenVideoPlayer(fileName);
            return true;
        }
        return false;
    }

    *texture = file;
    return true;
}

bool FileCache::loadBinary(const QString &fileName, QByteArray *binary) const
{
    if (!binary || FileDialog::isEmptyOrUntitled(fileName))
        return false;

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return false;
    *binary = file.readAll();
    return true;
}

bool FileCache::getSource(const QString &fileName, QString *source) const
{
    Q_ASSERT(source);
    QMutexLocker lock(&mMutex);
    if (mSources.contains(fileName)) {
        *source = mSources[fileName];
        return true;
    }

    addFileSystemWatch(fileName);
    if (!loadSource(fileName, source))
        return false;
    mSources[fileName] = *source;
    return true;
}

bool FileCache::getTexture(const QString &fileName, TextureData *texture) const
{
    Q_ASSERT(texture);
    QMutexLocker lock(&mMutex);
    if (mTextures.contains(fileName)) {
        *texture = mTextures[fileName];
        return true;
    }

    addFileSystemWatch(fileName);
    if (!loadTexture(fileName, texture))
        return false;
    mTextures[fileName] = *texture;
    return true;
}

bool FileCache::updateTexture(const QString &fileName, TextureData texture) const
{
    QMutexLocker lock(&mMutex);
    if (!mTextures.contains(fileName))
        return false;
    mTextures[fileName] = std::move(texture);
    return true;
}

bool FileCache::getBinary(const QString &fileName, QByteArray *binary) const
{
    Q_ASSERT(binary);
    QMutexLocker lock(&mMutex);
    if (mBinaries.contains(fileName)) {
        *binary = mBinaries[fileName];
        return true;
    }

    addFileSystemWatch(fileName);
    if (!loadBinary(fileName, binary))
        return false;
    mBinaries[fileName] = *binary;
    return true;
}

void FileCache::addFileSystemWatch(const QString &fileName, bool changed) const
{
    if (!FileDialog::isEmptyOrUntitled(fileName))
        mFileSystemWatchesToAdd[fileName] |= changed;
}

void FileCache::handleFileSystemFileChanged(const QString &fileName)
{
    QMutexLocker lock(&mMutex);
    addFileSystemWatch(fileName, true);
}

void FileCache::updateFileSystemWatches()
{
    QMutexLocker lock(&mMutex);
    auto filesChanged = QSet<QString>();
    for (auto it = mFileSystemWatchesToAdd.begin(); it != mFileSystemWatchesToAdd.end(); ) {
        const auto &fileName = it.key();
        const auto &changed = it.value();
        mFileSystemWatcher.removePath(it.key());
        if (QFileInfo(fileName).exists() &&
            mFileSystemWatcher.addPath(fileName)) {
            if (changed)
                filesChanged.insert(fileName);
            it = mFileSystemWatchesToAdd.erase(it);
        }
        else {
            ++it;
        }
    }
    lock.unlock();

    const auto getEditor = [](const auto &fileName) -> IEditor* {
        auto &editorManager = Singletons::editorManager();
        if (auto editor = editorManager.getSourceEditor(fileName))
            return editor;
        if (auto editor = editorManager.getBinaryEditor(fileName))
            return editor;
        else if (auto editor = editorManager.getTextureEditor(fileName))
            return editor;
        return nullptr;
    };

    for (const auto &fileName : qAsConst(filesChanged)) {
        if (auto editor = getEditor(fileName)) {
            if (!mEditorSaveAdvertised.remove(fileName))
                editor->reload();
        }
        else {
            QMutexLocker lock(&mMutex);
            mSources.remove(fileName);
            mBinaries.remove(fileName);
            mTextures.remove(fileName);
        }
        Q_EMIT fileChanged(fileName);
    }

    // enqueue update
    mUpdateFileSystemWatchesTimer.start(5);
}

void FileCache::asyncOpenVideoPlayer(const QString &fileName) const
{
    Q_EMIT videoPlayerRequested(fileName, QPrivateSignal());
}

void FileCache::handleVideoPlayerRequested(const QString &fileName)
{
    Q_ASSERT(onMainThread());
    auto videoPlayer = new VideoPlayer(fileName);
    connect(videoPlayer, &VideoPlayer::loadingFinished,
        this, &FileCache::handleVideoPlayerLoaded);
}

void FileCache::handleVideoPlayerLoaded()
{
    Q_ASSERT(onMainThread());
    auto videoPlayer = qobject_cast<VideoPlayer*>(QObject::sender());
    if (videoPlayer->width()) {
        if (mVideosPlaying)
            videoPlayer->play();
        mVideoPlayers[videoPlayer->fileName()].reset(videoPlayer);
    }
    else {
        videoPlayer->deleteLater();
    }
}

void FileCache::playVideoFiles()
{
    Q_ASSERT(onMainThread());
    for (const auto &videoPlayer : mVideoPlayers)
        videoPlayer.second->play();
    mVideosPlaying = true;
}

void FileCache::pauseVideoFiles()
{
    Q_ASSERT(onMainThread());
    for (const auto &videoPlayer : mVideoPlayers)
        videoPlayer.second->pause();
    mVideosPlaying = false;
}

void FileCache::rewindVideoFiles()
{
    Q_ASSERT(onMainThread());
    for (const auto &videoPlayer : mVideoPlayers)
        videoPlayer.second->rewind();
}
