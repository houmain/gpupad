#pragma once

#include "MediaFrame.h"
#include <QByteArray>
#include <QObject>
#include <cstdint>

class TextureData;
using ItemId = int;

struct MediaFrameRequest
{
    uint64_t audioSampleBase{};
    int audioFrameCount{};
    qint64 videoStartTime{};
    qint64 videoEndTime{};
    qint64 audioStartTime{};
    int audioSampleRate{};
    bool isFinish{};
};

class MediaFrameWriter : public QObject
{
    Q_OBJECT

public:
    explicit MediaFrameWriter(QObject *parent = nullptr);

    void setRecording(ItemId textureItemId);

    bool recording() const { return mRecording; }
    ItemId textureItemId() const { return mTextureItemId; }
    bool textureRequested(ItemId itemId) const;

    void beginFrame(MediaFrameRequest request);
    uint64_t audioSampleBase() const;
    int audioFrameCount() const;
    void writeAudio(QByteArray samples, int volume);
    void writeTexture(TextureData texture);
    void endFrame();

Q_SIGNALS:
    void frameReady(MediaFrame frame);

private:
    MediaFrameRequest mRequest;
    QByteArray mAudioBuffer;
    QImage mImage;
    ItemId mTextureItemId{};
    bool mRecording{};
};
