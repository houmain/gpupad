#include "FileCache.h"
#include "editors/EditorManager.h"
#include "editors/SourceEditor.h"
#include "editors/ImageEditor.h"
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

        if (auto editor = editorManager.getImageEditor(fileName))
            mImages[editor->fileName()] = editor->image();
        else
            mImages.remove(fileName);
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

bool FileCache::getImage(const QString &fileName, ImageData *image) const
{
    Q_ASSERT(image);
    QMutexLocker lock(&mMutex);
    if (mImages.contains(fileName)) {
        *image = mImages[fileName];
        return true;
    }

    if (!ImageEditor::load(fileName, image))
        return false;
    mImages[fileName] = *image;
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
