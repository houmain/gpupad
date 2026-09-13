#pragma once

#if defined(MULTIMEDIA_ENABLED)

#  include "AudioStream.h"
#  include "KissFFT.h"
#  include <vector>

class AudioSpectrum final : public AudioStream
{
    Q_OBJECT

public:
    explicit AudioSpectrum(MediaSource source, QObject *parent = nullptr);
    void publishFrame(uint64_t sampleBase) override;

private:
    int mAmplitudeCount{ };
    int mSignalSize{ };
    KissFFT mFft;
    std::vector<float> mSignal;
};

#endif // defined(MULTIMEDIA_ENABLED)
