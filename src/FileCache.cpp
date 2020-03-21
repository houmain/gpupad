#include "FileCache.h"
#include "Singletons.h"
#include "editors/EditorManager.h"
#include "editors/SourceEditor.h"
#include "editors/TextureEditor.h"
#include "editors/BinaryEditor.h"

FileCache::FileCache(QObject *parent) : QObject(parent)
{
    connect(&mFileSystemWatcher, &QFileSystemWatcher::fileChanged,
        this, &FileCache::handleFileSystemFileChanged);
    connect(&mFileSystemWatchUpdateTimer, &QTimer::timeout,
        this, &FileCache::addFileSystemWatches);
    mFileSystemWatchUpdateTimer.setInterval(500);
}

void FileCache::invalidateEditorFile(const QString &fileName)
{
    mEditorFilesInvalidated.insert(fileName);
    emit fileChanged(fileName);
}

void FileCache::updateEditorFiles()
{
    QMutexLocker lock(&mMutex);
    auto &editorManager = Singletons::editorManager();
    for (const auto &fileName : mEditorFilesInvalidated) {
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

bool FileCache::getSource(const QString &fileName, QString *source) const
{
    Q_ASSERT(source);
    QMutexLocker lock(&mMutex);
    if (mSources.contains(fileName)) {
        *source = mSources[fileName];
        return true;
    }

    addFileSystemWatch(fileName);
    if (!SourceEditor::load(fileName, source))
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
    if (!TextureEditor::load(fileName, texture))
        return false;
    mTextures[fileName] = *texture;
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
    if (!BinaryEditor::load(fileName, binary))
        return false;
    mBinaries[fileName] = *binary;
    return true;
}

void FileCache::addFileSystemWatch(const QString &fileName) const
{
    if (FileDialog::isEmptyOrUntitled(fileName) ||
        mFileSystemWatcher.files().contains(fileName))
        return;

    if (!QFileInfo(fileName).exists() ||
        !mFileSystemWatcher.addPath(fileName))
        mFileSystemWatchesToAdd.insert(fileName);
}

void FileCache::addFileSystemWatches()
{
    for (auto fileName : std::exchange(mFileSystemWatchesToAdd, { }))
        addFileSystemWatch(fileName);
}

void FileCache::removeFileSystemWatch(const QString &fileName) const
{
    mFileSystemWatcher.removePath(fileName);
}

void FileCache::handleFileSystemFileChanged(const QString &fileName)
{
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

    if (auto editor = getEditor(fileName)) {
        editor->reload();
    }
    else {
        QMutexLocker lock(&mMutex);
        mSources.remove(fileName);
        mBinaries.remove(fileName);
        mTextures.remove(fileName);
    }

    emit fileChanged(fileName);

    removeFileSystemWatch(fileName);
    addFileSystemWatch(fileName);
}
