#include "FileCache.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include "editors/EditorManager.h"
#include "editors/source/SourceEditor.h"
#include "editors/texture/TextureEditor.h"
#include "editors/binary/BinaryEditor.h"
#include <QThread>
#include <QTextStream>

namespace
{
    const auto textureUpdateInterval = 5;
    const auto nonTextureUpdateInterval = 1000;

    void setUtf8Encoding(QTextStream &stream) 
    {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        stream.setCodec("UTF-8");
#else
        stream.setEncoding(QStringConverter::Utf8);
#endif
    }

    void setSystemEncoding(QTextStream &stream) 
    {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        stream.setCodec("Windows-1250");
#else
        stream.setEncoding(QStringConverter::System);
#endif
    }

    qsizetype countUnprintable(const QString &string) {
        return std::count_if(string.constBegin(), string.constEnd(),
            [](QChar c) {
                const auto code = c.unicode();
                if (code == 0xFFFD ||
                    (code < 31 &&
                      code != '\n' &&
                      code != '\r' &&
                      code != '\t'))
                    return true;
                return false;
            });
    };

    bool loadSource(const QString &fileName, QString *source)
    {
        const auto detectEncodingSize = 1024 * 10;
        if (!source)
            return false;

        if (FileDialog::isEmptyOrUntitled(fileName)) {
            *source = "";
            return true;
        }
        QFile file(fileName);
        if (!file.open(QFile::ReadOnly | QFile::Text))
            return false;

        QTextStream stream(&file);
        setUtf8Encoding(stream);
        auto string = stream.read(detectEncodingSize);

        const auto unprintableUtf8 = countUnprintable(string);
        if (unprintableUtf8 > 0) {
            setSystemEncoding(stream);
            stream.seek(0);
            string = stream.read(detectEncodingSize);
            
            if (countUnprintable(string) > 0) {
                // allow up to 5 percent mojibake
                if (unprintableUtf8 * 20 > string.length())
                    return false;
                setUtf8Encoding(stream);
                stream.seek(0);
                string = stream.read(detectEncodingSize);
            }
        }

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
        else
            Q_EMIT loadingFailed(fileName);
    }

    void loadTexture(const QString &fileName, bool flipVertically) 
    {
        auto texture = TextureData();
        if (::loadTexture(fileName, flipVertically, &texture))
            Q_EMIT textureLoaded(fileName, flipVertically, std::move(texture));
        else
            Q_EMIT loadingFailed(fileName);
    }

    void loadBinary(const QString &fileName) 
    {
        auto binary = QByteArray();
        if (::loadBinary(fileName, &binary))
            Q_EMIT binaryLoaded(fileName, std::move(binary));
        else
            Q_EMIT loadingFailed(fileName);
    }

Q_SIGNALS:
    void sourceLoaded(const QString &fileName, QString source);
    void textureLoaded(const QString &fileName, bool flipVertically, TextureData texture);
    void binaryLoaded(const QString &fileName, QByteArray binary);
    void loadingFailed(const QString &fileName);
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
    connect(backgroundLoader, &BackgroundLoader::loadingFailed,
        this, &FileCache::handleReloadingFailed);

    connect(&mBackgroundLoaderThread, &QThread::finished,
        backgroundLoader, &QObject::deleteLater);

    mBackgroundLoaderThread.start();
    backgroundLoader->moveToThread(&mBackgroundLoaderThread);

    mUpdateFileSystemWatchesTimer.setInterval(textureUpdateInterval);
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
    Q_ASSERT(isNativeCanonicalFilePath(fileName));
    QMutexLocker lock(&mMutex);
    purgeFile(fileName);
}

void FileCache::handleEditorFileChanged(const QString &fileName, bool emitFileChanged)
{
    Q_ASSERT(onMainThread());
    Q_ASSERT(isNativeCanonicalFilePath(fileName));
    mEditorFilesChanged.insert(fileName);
    if (emitFileChanged)
        Q_EMIT fileChanged(fileName);
}

void FileCache::handleEditorSave(const QString &fileName)
{
    Q_ASSERT(onMainThread());
    Q_ASSERT(isNativeCanonicalFilePath(fileName));
    QMutexLocker lock(&mMutex);
    updateFromEditor(fileName);
    mEditorSaveAdvertised.insert(fileName);
}

bool FileCache::updateFromEditor(const QString &fileName)
{
    auto &editorManager = Singletons::editorManager();
    if (auto editor = editorManager.getSourceEditor(fileName)) {
        mSources[fileName] = editor->source();
        return true;
    }
    if (auto editor = editorManager.getBinaryEditor(fileName)) {
        mBinaries[fileName] = editor->data();
        return true;
    }
    if (auto editor = editorManager.getTextureEditor(fileName)) {
        const auto &texture = editor->texture();
        mTextures[TextureKey(fileName, texture.flippedVertically())] = texture;
        return true;
    }
    return false;
}

void FileCache::updateFromEditors()
{
    Q_ASSERT(onMainThread());
    QMutexLocker lock(&mMutex);
    for (const auto &fileName : qAsConst(mEditorFilesChanged))
        if (!updateFromEditor(fileName))
            purgeFile(fileName);
    mEditorFilesChanged.clear();
}

bool FileCache::getSource(const QString &fileName, QString *source) const
{
    Q_ASSERT(source);
    Q_ASSERT(isNativeCanonicalFilePath(fileName));
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
    Q_ASSERT(isNativeCanonicalFilePath(fileName));
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

bool FileCache::updateTexture(const QString &fileName, bool flippedVertically, TextureData texture)
{
    Q_ASSERT(!texture.isNull());
    Q_ASSERT(isNativeCanonicalFilePath(fileName));
    QMutexLocker lock(&mMutex);

    const auto key = TextureKey(fileName, flippedVertically);
    if (!mTextures.contains(key))
        return false;
    mTextures[key] = std::move(texture);
    lock.unlock();

    if (auto editor = Singletons::editorManager().getEditor(fileName))
        editor->load();
    
    Q_EMIT fileChanged(fileName);

    return true;
}

bool FileCache::getBinary(const QString &fileName, QByteArray *binary) const
{
    Q_ASSERT(binary);
    Q_ASSERT(isNativeCanonicalFilePath(fileName));
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
    Q_ASSERT(isNativeCanonicalFilePath(fileName));
    if (!FileDialog::isEmptyOrUntitled(fileName))
        mFileSystemWatchesToAdd[fileName] |= changed;
}

void FileCache::updateFileSystemWatches()
{
    Q_ASSERT(onMainThread());
    QMutexLocker lock(&mMutex);

    const auto updatingNonTextures = ((mFileSystemWatcherUpdate++ % 
        (nonTextureUpdateInterval / textureUpdateInterval)) == 0);

    for (auto it = mFileSystemWatchesToAdd.begin(); it != mFileSystemWatchesToAdd.end(); ) {
        const auto &fileName = it.key();
        const auto &changed = it.value();

        auto deferUpdate = (!updatingNonTextures && 
            (mSources.contains(fileName) || mBinaries.contains(fileName)));

        if (!deferUpdate) {
            if (!mFileSystemWatcher.files().contains(fileName)) {
                if (!mFileSystemWatcher.addPath(fileName)) {
                    deferUpdate = true;

                    // inform editor that file does no longer exist
                    if (auto editor = Singletons::editorManager().getEditor(fileName))
                        editor->setModified();
                }
            }
            else if (!QFileInfo::exists(fileName)) {
                mFileSystemWatcher.removePath(fileName);
            }
        }

        if (!deferUpdate) {
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
    }
    else if (mTextures.contains({ fileName, true })) {
        Q_EMIT reloadTexture(fileName, true, QPrivateSignal());
    } 
    else if (mTextures.contains({ fileName, false })) {
        Q_EMIT reloadTexture(fileName, false, QPrivateSignal());
    } 
    else if (mBinaries.contains(fileName)) {
        Q_EMIT reloadBinary(fileName, QPrivateSignal());
    }
    else {
        return false;
    }
    mUpdateFileSystemWatchesTimer.stop();
    return true;
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

    if (auto editor = Singletons::editorManager().getEditor(fileName))
        editor->load();

    Q_EMIT fileChanged(fileName);
    mUpdateFileSystemWatchesTimer.start();
}

void FileCache::handleTextureReloaded(const QString &fileName, bool flipVertically, TextureData texture) 
{
    Q_ASSERT(onMainThread());
    QMutexLocker lock(&mMutex);
    mTextures[TextureKey(fileName, flipVertically)] = texture;
    lock.unlock();

    if (auto editor = Singletons::editorManager().getEditor(fileName))
        editor->load();
    
    Q_EMIT fileChanged(fileName);
    mUpdateFileSystemWatchesTimer.start();
}

void FileCache::handleBinaryReloaded(const QString &fileName, QByteArray binary) 
{
    Q_ASSERT(onMainThread());
    QMutexLocker lock(&mMutex);
    mBinaries[fileName] = binary;
    lock.unlock();

    if (auto editor = Singletons::editorManager().getEditor(fileName))
        editor->load();

    Q_EMIT fileChanged(fileName);
    mUpdateFileSystemWatchesTimer.start();
}

void FileCache::handleReloadingFailed(const QString &fileName) 
{
    Q_ASSERT(onMainThread());
    mUpdateFileSystemWatchesTimer.start();
}

#include "FileCache.moc"
