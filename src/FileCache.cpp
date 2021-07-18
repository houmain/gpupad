#include "FileCache.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include "editors/EditorManager.h"
#include "editors/SourceEditor.h"
#include "editors/TextureEditor.h"
#include "editors/BinaryEditor.h"
#include <QThread>
#include <QTextStream>

namespace
{
    bool loadSource(const QString &fileName, QString *source)
    {
        const auto detectEncodingSize = 1024;
        if (!source)
            return false;

        if (FileDialog::isEmptyOrUntitled(fileName)) {
            *source = "";
            return true;
        }
        QFile file(fileName);
        if (!file.open(QFile::ReadOnly))
            return false;

        QTextStream stream(&file);
        auto string = stream.read(detectEncodingSize);

        const auto isUnprintable = [&]() {
            return (std::find_if(string.constBegin(), string.constEnd(),
                [](QChar c) {
                    const auto code = c.unicode();
                    if (code == 0xFFFD ||
                        (code < 31 &&
                         code != '\n' &&
                         code != '\r' &&
                         code != '\t'))
                        return true;
                    return false;
                }) != string.constEnd());
        };

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        if (isUnprintable()) {
            stream.setCodec("Windows-1250");
            stream.seek(0);
            string = stream.read(detectEncodingSize);
        }
#endif

        if (isUnprintable())
            return false;

        *source = string + stream.readAll();
        return true;
    }

    bool loadTexture(const QString &fileName, bool flipVertically, TextureData *texture)
    {
        if (!texture || FileDialog::isEmptyOrUntitled(fileName))
            return false;

        auto file = TextureData();
        if (!file.load(fileName, flipVertically))
            return false;

        *texture = file;
        return true;
    }

    bool loadBinary(const QString &fileName, QByteArray *binary)
    {
        if (!binary || FileDialog::isEmptyOrUntitled(fileName))
            return false;

        QFile file(fileName);
        if (!file.open(QFile::ReadOnly))
            return false;
        *binary = file.readAll();
        return true;
    }

    bool isFileInUse(const QString &fileName)
    {
        if (Singletons::editorManager().getEditor(fileName))
            return true;
        auto fileInUse = false;
        Singletons::sessionModel().forEachFileItem(
            [&](const FileItem& item) { fileInUse |= (item.fileName == fileName); });
        return fileInUse;
    }
} // namespace

class FileCache::BackgroundLoader final : public QObject 
{
    Q_OBJECT

public Q_SLOTS:
    void loadSource(const QString &fileName) 
    {
        auto source = QString();
        if (::loadSource(fileName, &source))
            Q_EMIT sourceLoaded(fileName, std::move(source));
    }

    void loadTexture(const QString &fileName, bool flipVertically) 
    {
        auto texture = TextureData();
        if (::loadTexture(fileName, flipVertically, &texture))
            Q_EMIT textureLoaded(fileName, flipVertically, std::move(texture));
    }

    void loadBinary(const QString &fileName) 
    {
        auto binary = QByteArray();
        if (::loadBinary(fileName, &binary))
            Q_EMIT binaryLoaded(fileName, std::move(binary));
    }

Q_SIGNALS:
    void sourceLoaded(const QString &fileName, QString source);
    void textureLoaded(const QString &fileName, bool flipVertically, TextureData texture);
    void binaryLoaded(const QString &fileName, QByteArray binary);
};

FileCache::FileCache(QObject *parent) 
    : QObject(parent)
{
    qRegisterMetaType<TextureData>();

    connect(&mFileSystemWatcher, &QFileSystemWatcher::fileChanged,
        this, &FileCache::handleFileSystemFileChanged);
    connect(&mUpdateFileSystemWatchesTimer, &QTimer::timeout,
        this, &FileCache::updateFileSystemWatches);

    auto backgroundLoader = new BackgroundLoader();
    connect(this, &FileCache::reloadSource,
        backgroundLoader, &BackgroundLoader::loadSource);
    connect(this, &FileCache::reloadTexture,
        backgroundLoader, &BackgroundLoader::loadTexture);
    connect(this, &FileCache::reloadBinary,
        backgroundLoader, &BackgroundLoader::loadBinary);

    connect(backgroundLoader, &BackgroundLoader::sourceLoaded,
        this, &FileCache::handleSourceReloaded);
    connect(backgroundLoader, &BackgroundLoader::textureLoaded,
        this, &FileCache::handleTextureReloaded);
    connect(backgroundLoader, &BackgroundLoader::binaryLoaded,
        this, &FileCache::handleBinaryReloaded);

    connect(&mBackgroundLoaderThread, &QThread::finished,
        backgroundLoader, &QObject::deleteLater);

    mBackgroundLoaderThread.start();
    backgroundLoader->moveToThread(&mBackgroundLoaderThread);

    mUpdateFileSystemWatchesTimer.setInterval(5);
    mUpdateFileSystemWatchesTimer.setSingleShot(false);
    mUpdateFileSystemWatchesTimer.start();
}

FileCache::~FileCache() 
{
    mBackgroundLoaderThread.quit();
    mBackgroundLoaderThread.wait();
}

void FileCache::invalidateFile(const QString &fileName) 
{
    QMutexLocker lock(&mMutex);
    purgeFile(fileName);
}

void FileCache::handleEditorFileChanged(const QString &fileName, bool emitFileChanged)
{
    Q_ASSERT(onMainThread());
    mEditorFilesChanged.insert(fileName);
    if (emitFileChanged)
        Q_EMIT fileChanged(fileName);
}

void FileCache::handleEditorSave(const QString &fileName)
{
    Q_ASSERT(onMainThread());
    mEditorSaveAdvertised.insert(fileName);
}

void FileCache::updateEditorFiles()
{
    Q_ASSERT(onMainThread());
    QMutexLocker lock(&mMutex);

    auto &editorManager = Singletons::editorManager();
    for (const auto &fileName : qAsConst(mEditorFilesChanged)) {
        if (auto editor = editorManager.getSourceEditor(fileName)) {
            mSources[fileName] = editor->source();
        }
        else if (auto editor = editorManager.getBinaryEditor(fileName)) {
            mBinaries[fileName] = editor->data();
        }
        else if (auto editor = editorManager.getTextureEditor(fileName)) {
            const auto &texture = editor->texture();
            mTextures[TextureKey(fileName, texture.flippedVertically())] = texture;
        }
        else {
            purgeFile(fileName);
        }
    }
    mEditorFilesChanged.clear();
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

bool FileCache::getTexture(const QString &fileName, bool flipVertically, TextureData *texture) const
{
    Q_ASSERT(texture);
    QMutexLocker lock(&mMutex);

    const auto key = TextureKey(fileName, flipVertically);
    if (mTextures.contains(key)) {
        *texture = mTextures[key];
        return true;
    }

    addFileSystemWatch(fileName);

    if (FileDialog::isVideoFileName(fileName)) {
        texture->create(QOpenGLTexture::Target2D,
            QOpenGLTexture::RGB8_UNorm, 1, 1, 1, 1, 1);
        texture->clear();
        Q_EMIT videoPlayerRequested(fileName, flipVertically);
    }
    else if (!loadTexture(fileName, flipVertically, texture)) {
        return false;
    }
    mTextures[key] = *texture;
    return true;
}

bool FileCache::updateTexture(const QString &fileName, bool flippedVertically, TextureData texture) const
{
    Q_ASSERT(!texture.isNull());
    QMutexLocker lock(&mMutex);

    const auto key = TextureKey(fileName, flippedVertically);
    if (!mTextures.contains(key))
        return false;
    mTextures[key] = std::move(texture);
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

void FileCache::handleFileSystemFileChanged(const QString &fileName)
{
    Q_ASSERT(onMainThread());
    addFileSystemWatch(fileName, true);
}

void FileCache::addFileSystemWatch(const QString &fileName, bool changed) const
{
    if (!FileDialog::isEmptyOrUntitled(fileName))
        mFileSystemWatchesToAdd[fileName] |= changed;
}

void FileCache::updateFileSystemWatches()
{
    Q_ASSERT(onMainThread());
    QMutexLocker lock(&mMutex);

    for (auto it = mFileSystemWatchesToAdd.begin(); it != mFileSystemWatchesToAdd.end(); ) {
        const auto &fileName = it.key();
        const auto &changed = it.value();
        mFileSystemWatcher.removePath(it.key());
        if (QFileInfo(fileName).exists() &&
            mFileSystemWatcher.addPath(fileName)) {
            if (changed && !mEditorSaveAdvertised.remove(fileName)) {
                if (!reloadFileInBackground(fileName)) {
                    purgeFile(fileName);
                    Q_EMIT fileChanged(fileName);
                }
            }
            it = mFileSystemWatchesToAdd.erase(it);
        }
        else {
            ++it;
        }
    }
}

bool FileCache::reloadFileInBackground(const QString &fileName) 
{
    if (mSources.contains(fileName)) {
        Q_EMIT reloadSource(fileName, QPrivateSignal());
        return true;
    }
    if (mTextures.contains({ fileName, true })) {
        Q_EMIT reloadTexture(fileName, true, QPrivateSignal());
        return true;
    } 
    if (mTextures.contains({ fileName, false })) {
        Q_EMIT reloadTexture(fileName, false, QPrivateSignal());
        return true;
    } 
    if (mBinaries.contains(fileName)) {
        Q_EMIT reloadBinary(fileName, QPrivateSignal());
        return true;
    }
    return false;
}

void FileCache::purgeFile(const QString &fileName)
{
    mSources.remove(fileName);
    mBinaries.remove(fileName);
    mTextures.remove(TextureKey(fileName, true));
    mTextures.remove(TextureKey(fileName, false));
}

void FileCache::handleSourceReloaded(const QString &fileName, QString source) 
{
    Q_ASSERT(onMainThread());
    QMutexLocker lock(&mMutex);

    mSources[fileName] = source;
    lock.unlock();

    if (auto editor = Singletons::editorManager().getSourceEditor(fileName))
        editor->load();

    Q_EMIT fileChanged(fileName);
}

void FileCache::handleTextureReloaded(const QString &fileName, bool flipVertically, TextureData texture) 
{
    Q_ASSERT(onMainThread());
    QMutexLocker lock(&mMutex);

    mTextures[TextureKey(fileName, flipVertically)] = texture;
    lock.unlock();

    if (auto editor = Singletons::editorManager().getTextureEditor(fileName))
        editor->load();
    
    Q_EMIT fileChanged(fileName);
}

void FileCache::handleBinaryReloaded(const QString &fileName, QByteArray binary) 
{
    Q_ASSERT(onMainThread());
    QMutexLocker lock(&mMutex);

    mBinaries[fileName] = binary;
    lock.unlock();

    if (auto editor = Singletons::editorManager().getBinaryEditor(fileName))
        editor->load();

    Q_EMIT fileChanged(fileName);
}

#include "FileCache.moc"
