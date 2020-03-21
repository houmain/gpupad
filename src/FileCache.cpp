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

    if (!SourceEditor::load(fileName, source))
        return false;
    mSources[fileName] = *source;
    addFileSystemWatch(fileName);
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

    if (!TextureEditor::load(fileName, texture))
        return false;
    mTextures[fileName] = *texture;
    addFileSystemWatch(fileName);
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

    if (!BinaryEditor::load(fileName, binary))
        return false;
    mBinaries[fileName] = *binary;
    addFileSystemWatch(fileName);
    return true;
}

void FileCache::addFileSystemWatch(const QString &fileName) const
{
    if (FileDialog::isEmptyOrUntitled(fileName) ||
        mFileSystemWatcher.files().contains(fileName))
        return;

    mFileSystemWatcher.addPath(fileName);
}

void FileCache::removeFileSystemWatch(const QString &fileName) const
{
    mFileSystemWatcher.removePath(fileName);
}

void FileCache::handleFileSystemFileChanged(const QString &fileName)
{
    auto &editorManager = Singletons::editorManager();
    if (auto editor = editorManager.getSourceEditor(fileName)) {
        editor->reload();
    }
    else if (auto editor = editorManager.getBinaryEditor(fileName)) {
        editor->reload();
    }
    else if (auto editor = editorManager.getTextureEditor(fileName)) {
        editor->reload();
    }
    else {
        QMutexLocker lock(&mMutex);
        mSources.remove(fileName);
        mBinaries.remove(fileName);
        mTextures.remove(fileName);
    }

    removeFileSystemWatch(fileName);
    addFileSystemWatch(fileName);

    emit fileChanged(fileName);
}
