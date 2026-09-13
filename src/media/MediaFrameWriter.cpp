#include "MediaFrameWriter.h"
#include "TextureData.h"
#include <QCoreApplication>
#include <algorithm>
#include <cmath>
#include <utility>

MediaFrameWriter::MediaFrameWriter(QObject *parent) : QObject(parent) { }

void MediaFrameWriter::setRecording(ItemId textureItemId)
{
    mTextureItemId = textureItemId;
    mRecording = true;
}

void MediaFrameWriter::beginFrame(MediaFrameRequest request)
{
    mAudioBuffer.clear();
    mImage = { };
    request.audioFrameCount = std::max(request.audioFrameCount, 0);
    mRequest = std::move(request);
}

bool MediaFrameWriter::textureRequested(ItemId itemId) const
{
    return recording() && mTextureItemId != 0 && itemId == mTextureItemId;
}

uint64_t MediaFrameWriter::audioSampleBase() const
{
    return mRequest.audioSampleBase;
}

int MediaFrameWriter::audioFrameCount() const
{
    return mRequest.audioFrameCount;
}

void MediaFrameWriter::writeAudio(QByteArray samples, int volume)
{
    if (audioFrameCount() == 0)
        return;
    const auto expectedSize = audioFrameCount() * BytesPerFrame;
    Q_ASSERT(samples.size() == expectedSize);
    if (samples.size() != expectedSize)
        return;

    if (mAudioBuffer.isEmpty())
        mAudioBuffer = QByteArray(expectedSize, '\0');

    const auto gain = static_cast<float>(std::clamp(volume, 0, 100)) / 100.0f;
    auto *dest = reinterpret_cast<float *>(mAudioBuffer.data());
    const auto *source = reinterpret_cast<const float *>(samples.constData());
    const auto valueCount = samples.size()
        / static_cast<qsizetype>(sizeof(float));
    for (auto index = qsizetype{ 0 }; index < valueCount; ++index)
        if (std::isfinite(source[index]))
            dest[index] += source[index] * gain;
}

void MediaFrameWriter::writeTexture(TextureData texture)
{
    if (mTextureItemId == 0 || texture.isNull())
        return;

    mImage = texture.reoriented(TextureData::RowOrder::TopToBottom)
                 .convert(Texture::Format::RGBA8_UNorm)
                 .toImage();
    Q_ASSERT(!mImage.isNull());
}

void MediaFrameWriter::endFrame()
{
    auto *values = reinterpret_cast<float *>(mAudioBuffer.data());
    const auto valueCount = mAudioBuffer.size()
        / static_cast<qsizetype>(sizeof(float));
    for (auto index = qsizetype{ 0 }; index < valueCount; ++index)
        values[index] = std::isfinite(values[index])
            ? std::clamp(values[index], -1.0f, 1.0f)
            : 0.0f;
    auto audio = std::move(mAudioBuffer);
    if (!recording()) {
        const auto rate = std::max(mRequest.audioSampleRate, 1);
        const auto audioStartTime =
            static_cast<qint64>(audioSampleBase() * 1'000'000.0 / rate);
        Q_EMIT frameReady(MediaFrame({ }, std::move(audio), 0, 0,
            audioStartTime, rate, false));
        return;
    }

    if (audio.isEmpty() && mRequest.audioFrameCount > 0)
        audio = QByteArray(mRequest.audioFrameCount * BytesPerFrame, '\0');

    auto frame = MediaFrame(std::move(mImage), std::move(audio),
        mRequest.videoStartTime, mRequest.videoEndTime, mRequest.audioStartTime,
        std::max(mRequest.audioSampleRate, 1), mRequest.isFinish);
    Q_EMIT frameReady(std::move(frame));
}
