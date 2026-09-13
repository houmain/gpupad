#include "AudioSpectrum.h"

#if defined(MULTIMEDIA_ENABLED)

#  include "TextureData.h"
#  include <algorithm>
#  include <cstring>
#  include <utility>

AudioSpectrum::AudioSpectrum(MediaSource source, QObject *parent)
    : AudioStream(std::move(source), parent)
    , mAmplitudeCount(textureResolution().width())
    , mSignalSize(mAmplitudeCount * 2)
    , mFft(mSignalSize, KissFFT::WindowType::hann)
    , mSignal(mSignalSize)
{
}

void AudioSpectrum::publishFrame(uint64_t sampleBase)
{
    const auto soundBuffer = samples(sampleBase, mSignalSize);
    if (soundBuffer.isEmpty())
        return finishLoading();

    const auto *input =
        reinterpret_cast<const float *>(soundBuffer.constData());
    for (auto i = 0; i < mSignalSize; ++i)
        mSignal[i] = (input[i * 2] + input[i * 2 + 1]) * 0.5f;

    mFft.set_signal(mSignal.data());
    const auto &amplitudes = mFft.amplitudes();

    auto texture = TextureData{ };
    if (!texture.create(source().target, Texture::Format::R32F,
            mAmplitudeCount, 1, 1, 1, 1))
        return finishLoading();
    std::memcpy(texture.getWriteonlyData(), amplitudes.data(),
        mAmplitudeCount * sizeof(float));
    presentTexture(std::move(texture));
}

#endif // defined(MULTIMEDIA_ENABLED)
