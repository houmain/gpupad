
#include "MediaManager.h"
#include "FileCache.h"
#include "Singletons.h"
#include "VideoPlayer.h"
#include "Camera.h"
#include "AudioSamples.h"
#include "AudioSpectrum.h"
#include "AudioStream.h"
#include "DecodedAudio.h"
#include "FileDialog.h"
#include "editors/EditorManager.h"
#include "scripting/ScriptEngine.h"
#include "session/SessionModel.h"
#include <algorithm>

namespace {
    std::optional<MediaSource> getMediaSource(const Texture &texture)
    {
        if (texture.sourceType == Texture::SourceType::NoSource
            || FileDialog::isEmptyOrUntitled(texture.fileName))
            return std::nullopt;

        const auto width = std::max(1,
            Singletons::defaultScriptEngine().evaluateInt(texture.width,
                texture.id));
        return MediaSource{
            texture.fileName,
            texture.sourceType,
            texture.target,
            QSize(width, 1),
        };
    }
} // namespace

MediaManager::MediaManager(QObject *parent) : QObject(parent) { }

MediaManager::~MediaManager() = default;

#if defined(MULTIMEDIA_ENABLED)

void MediaManager::unloadAll()
{
    Q_ASSERT(onMainThread());
    mMediaStreams.clear();
    mDecodedAudio.clear();
    mHasAudio = false;
}

void MediaManager::unloadFile(const QString &fileName)
{
    Q_ASSERT(onMainThread());
    std::erase_if(mMediaStreams,
        [&](const auto &entry) { return entry.first.fileName == fileName; });
    std::erase_if(mDecodedAudio,
        [&](const auto &entry) { return entry.first.first == fileName; });
}

void MediaManager::unloadFiles(std::function<bool(const QString &)> predicate)
{
    for (auto it = mMediaStreams.begin(); it != mMediaStreams.end();)
        if (predicate(it->first.fileName)) {
            it = mMediaStreams.erase(it);
        } else {
            ++it;
        }
    std::erase_if(mDecodedAudio,
        [&](const auto &entry) { return predicate(entry.first.first); });
}

MediaStream *MediaManager::addMediaStreamOnce(MediaSource source)
{
    Q_ASSERT(onMainThread());
    auto it = mMediaStreams.find(source);
    if (it != mMediaStreams.end())
        return it->second.get();

    auto mediaStream = std::unique_ptr<MediaStream>{ };
    if (source.type == Texture::SourceType::AudioSpectrum) {
        mediaStream = std::make_unique<AudioSpectrum>(source);
    } else if (source.type == Texture::SourceType::AudioSamples) {
        mediaStream = std::make_unique<AudioSamples>(source);
    } else if (FileDialog::isCameraFileName(source.fileName)) {
        mediaStream = std::make_unique<Camera>(source);
    } else {
        mediaStream = std::make_unique<VideoPlayer>(source);
    }
    connect(mediaStream.get(), &MediaStream::readyChanged, this,
        [this, mediaStream = mediaStream.get()] {
            if (mediaStream->isReady())
                handleStreamReady(mediaStream);
        });

    mediaStream->seek(mTargetTime);

    it = mMediaStreams.emplace(std::move(source), std::move(mediaStream)).first;
    return it->second.get();
}

void MediaManager::handleMediaRequested(MediaSource source)
{
    addMediaStreamOnce(source);
}

bool MediaManager::prepareAudioSources(bool itemsChanged)
{
    Q_ASSERT(onMainThread());
    const auto &session = Singletons::sessionModel();
    const auto sampleRate = std::max(1, session.sessionItem().audioSampleRate);
    mHasAudio = false;
    session.forEachItem<Texture>([&](const Texture &texture) {
        if (!isAudioSource(texture.sourceType))
            return;
        const auto source = getMediaSource(texture);
        if (!source)
            return;
        mHasAudio = true;
        auto stream = qobject_cast<AudioStream *>(addMediaStreamOnce(*source));
        if (!stream)
            return;
        auto &audio = mDecodedAudio[{ source->fileName, sampleRate }];
        if (!audio)
            audio =
                std::make_shared<DecodedAudio>(source->fileName, sampleRate);
        stream->setDecodedAudio(audio);
    });
    if (itemsChanged)
        removeUnusedMediaStreams();
    return areAudioSourcesReady();
}

void MediaManager::removeUnusedMediaStreams()
{
    std::erase_if(mMediaStreams, [&](const auto &entry) {
        const auto &source = entry.first;
        if (source.type == Texture::SourceType::Video
            && Singletons::editorManager().getTextureEditor(source.fileName))
            return false;
        auto used = false;
        Singletons::sessionModel().forEachItem<Texture>(
            [&](const Texture &texture) {
                if (texture.fileName == source.fileName)
                    if (texture.sourceType == source.type)
                        used = (getMediaSource(texture) == source);
            });
        if (used)
            return false;
        Singletons::fileCache().invalidateMedia(source);
        return true;
    });
}

void MediaManager::prepareAudioFrame(uint64_t sampleBase)
{
    Q_ASSERT(onMainThread());
    for (const auto &entry : mMediaStreams) {
        const auto &mediaStream = entry.second;
        if (!mediaStream->isReady())
            continue;
        if (auto audioStream = qobject_cast<AudioStream *>(mediaStream.get()))
            audioStream->publishFrame(sampleBase);
    }
}

void MediaManager::handleStreamReady(MediaStream *mediaStream)
{
    Q_ASSERT(onMainThread() && mediaStream);
    const auto it = mMediaStreams.find(mediaStream->source());
    if (it == mMediaStreams.end() || it->second.get() != mediaStream)
        return;

    Singletons::fileCache().invalidateMedia(mediaStream->source());
    const auto audioSource = isAudioSource(it->first.type);
    if (!mediaStream->width()) {
        it->second.release()->deleteLater();
        mMediaStreams.erase(it);
    }
    if (audioSource && areAudioSourcesReady())
        Q_EMIT audioSourcesReady();
}

void MediaManager::seekToTargetTime()
{
    for (const auto &[source, mediaStream] : mMediaStreams)
        mediaStream->seek(mTargetTime);
}

void MediaManager::seek(double time)
{
    Q_ASSERT(onMainThread());
    const auto targetTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(time));
    if (mTargetTime == targetTime)
        return;
    mTargetTime = targetTime;
    seekToTargetTime();
}

void MediaManager::pause()
{
    Q_ASSERT(onMainThread());
    seekToTargetTime();
}

QByteArray MediaManager::audioSamples(const MediaSource &source,
    uint64_t sampleBase, int frameCount) const
{
    Q_ASSERT(onMainThread());
    const auto it = mMediaStreams.find(source);
    if (it == mMediaStreams.end() || !it->second->isReady())
        return { };
    const auto audioStream = qobject_cast<AudioStream *>(it->second.get());
    return (audioStream ? audioStream->samples(sampleBase, frameCount)
                        : QByteArray{ });
}

bool MediaManager::hasAudio() const
{
    Q_ASSERT(onMainThread());
    return mHasAudio;
}

bool MediaManager::areAudioSourcesReady() const
{
    Q_ASSERT(onMainThread());
    return std::none_of(mMediaStreams.begin(), mMediaStreams.end(),
        [](const auto &entry) {
            return isAudioSource(entry.first.type) && !entry.second->isReady();
        });
}

#else // !defined(MULTIMEDIA_ENABLED)

class MediaStream
{
};

void MediaManager::unloadAll() { }
void MediaManager::unloadFile(const QString &fileName) { }
void MediaManager::unloadFiles(std::function<bool(const QString &)> predicate)
{
}
void MediaManager::handleMediaRequested(MediaSource) { }
bool MediaManager::prepareAudioSources(bool itemsChanged)
{
    return true;
}
void MediaManager::prepareAudioFrame(uint64_t) { }
void MediaManager::seek(double time) { }
void MediaManager::pause() { }
QByteArray MediaManager::audioSamples(const MediaSource &, uint64_t, int) const
{
    return { };
}
bool MediaManager::hasAudio() const
{
    return false;
}
#endif // !defined(MULTIMEDIA_ENABLED)
