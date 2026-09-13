#pragma once

#include "MediaSource.h"
#include <QByteArray>
#include <QObject>
#include <cstdint>
#include <map>
#include <chrono>
#include <memory>

class MediaStream;
class DecodedAudio;

class MediaManager : public QObject
{
    Q_OBJECT
public:
    explicit MediaManager(QObject *parent = nullptr);
    MediaManager(const MediaManager &) = delete;
    MediaManager &operator=(const MediaManager &) = delete;
    ~MediaManager();

    void unloadAll();
    void unloadFile(const QString &fileName);
    void unloadFiles(std::function<bool(const QString &)> predicate);
    void seek(double time);
    void pause();
    void handleMediaRequested(MediaSource source);
    bool prepareAudioSources(bool itemsChanged);
    void prepareAudioFrame(uint64_t sampleBase);
    QByteArray audioSamples(const MediaSource &source, uint64_t sampleBase,
        int frameCount) const;
    bool hasAudio() const;

Q_SIGNALS:
    void audioSourcesReady();

private:
    void handleStreamReady(MediaStream *mediaStream);
    MediaStream *addMediaStreamOnce(MediaSource source);
    void removeUnusedMediaStreams();
    bool areAudioSourcesReady() const;
    void seekToTargetTime();

    std::map<MediaSource, std::unique_ptr<MediaStream>> mMediaStreams;
    std::map<std::pair<QString, int>, std::shared_ptr<DecodedAudio>> mDecodedAudio;
    std::chrono::milliseconds mTargetTime{ };
    bool mHasAudio{};
};
