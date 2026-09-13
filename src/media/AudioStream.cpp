#include "AudioStream.h"

#if defined(MULTIMEDIA_ENABLED)

#  include "DecodedAudio.h"
#  include <utility>

AudioStream::~AudioStream() = default;

AudioStream::AudioStream(MediaSource source, QObject *parent)
    : MediaStream(std::move(source), parent)
{
}

void AudioStream::setDecodedAudio(std::shared_ptr<DecodedAudio> audio)
{
    if (mDecodedAudio == audio)
        return;

    if (mDecodedAudio)
        disconnect(mDecodedAudio.get(), nullptr, this, nullptr);
    mDecodedAudio = std::move(audio);
    setReady(false);
    connect(mDecodedAudio.get(), &DecodedAudio::finished, this,
        [this] { publishFrame(0); });
    if (mDecodedAudio->isFinished())
        publishFrame(0);
}

QByteArray AudioStream::samples(uint64_t sampleBase, int frameCount) const
{
    return mDecodedAudio ? mDecodedAudio->samples(sampleBase, frameCount)
                         : QByteArray{ };
}

#endif // defined(MULTIMEDIA_ENABLED)
