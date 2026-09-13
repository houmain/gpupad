#pragma once

#if defined(MULTIMEDIA_ENABLED)

#  include "AudioStream.h"

class AudioSamples final : public AudioStream
{
public:
    explicit AudioSamples(MediaSource source, QObject *parent = nullptr);
    void publishFrame(uint64_t sampleBase) override;
};

#endif // defined(MULTIMEDIA_ENABLED)
