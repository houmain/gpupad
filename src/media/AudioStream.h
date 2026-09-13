#pragma once

#if defined(MULTIMEDIA_ENABLED)

#  include "MediaStream.h"
#  include <QByteArray>
#  include <cstdint>
#  include <memory>

class DecodedAudio;

class AudioStream : public MediaStream
{
    Q_OBJECT

public:
    ~AudioStream() override;

    void setDecodedAudio(std::shared_ptr<DecodedAudio> audio);
    void seek(std::chrono::milliseconds) final { }
    virtual void publishFrame(uint64_t sampleBase) = 0;
    QByteArray samples(uint64_t sampleBase, int frameCount) const;

protected:
    explicit AudioStream(MediaSource source, QObject *parent = nullptr);

    QSize textureResolution() const { return source().resolution(); }

private:
    std::shared_ptr<DecodedAudio> mDecodedAudio;
};

#endif // defined(MULTIMEDIA_ENABLED)
