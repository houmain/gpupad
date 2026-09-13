#pragma once

#if defined(MULTIMEDIA_ENABLED)

#  include <QObject>
#  include <QByteArray>
#  include <QString>
#  include <cstdint>

class QAudioDecoder;

class DecodedAudio final : public QObject
{
    Q_OBJECT
public:
    DecodedAudio(QString fileName, int sampleRate);

    bool isFinished() const { return mFinished; }
    QByteArray samples(uint64_t sampleBase, int frameCount) const;

Q_SIGNALS:
    void finished();

private:
    void handleBuffer();
    void finish(QString error = { });

    QAudioDecoder *mDecoder{ };
    QString mFileName;
    QByteArray mData;
    int mSampleRate{ };
    bool mFinished{ };
};

#endif // defined(MULTIMEDIA_ENABLED)
