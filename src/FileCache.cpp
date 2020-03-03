#include "FileCache.h"
#include "editors/EditorManager.h"
#include "editors/SourceEditor.h"
#include "editors/TextureEditor.h"
#include "editors/BinaryEditor.h"

FileCache::FileCache(QObject *parent) : QObject(parent)
{
}

void FileCache::update(EditorManager &editorManager,
                       const QSet<QString> &filesModified)
{
    QMutexLocker lock(&mMutex);

    for (const auto &fileName : filesModified) {
        if (auto editor = editorManager.getSourceEditor(fileName))
            mSources[editor->fileName()] = editor->source();
        else
            mSources.remove(fileName);

        if (auto editor = editorManager.getBinaryEditor(fileName))
            mBinaries[editor->fileName()] = editor->data();
        else
            mBinaries.remove(fileName);

        if (auto editor = editorManager.getTextureEditor(fileName))
            mTextures[editor->fileName()] = editor->texture();
        else
            mTextures.remove(fileName);
    }
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
    return true;
}
