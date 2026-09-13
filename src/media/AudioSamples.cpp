#include "AudioSamples.h"

#if defined(MULTIMEDIA_ENABLED)

#  include "TextureData.h"
#  include <cstring>
#  include <utility>

AudioSamples::AudioSamples(MediaSource source, QObject *parent)
    : AudioStream(std::move(source), parent)
{
}

void AudioSamples::publishFrame(uint64_t sampleBase)
{
    const auto frameCount = textureResolution().width();
    const auto sampleData = samples(sampleBase, frameCount);
    if (sampleData.isEmpty())
        return finishLoading();

    auto texture = TextureData{ };
    if (!texture.create(source().target,
            Texture::Format::RG32F, frameCount, 1, 1, 1, 1))
        return finishLoading();
    std::memcpy(texture.getWriteonlyData(), sampleData.constData(),
        static_cast<size_t>(sampleData.size()));
    presentTexture(std::move(texture));
}

#endif // defined(MULTIMEDIA_ENABLED)
