#pragma once

#if defined(MULTIMEDIA_ENABLED)

#  include "KissFFT.h"
#  include "VideoStream.h"
#  include <QMediaPlayer>
#  include <vector>

class QAudioBuffer;
class QAudioBufferOutput;
class QAudioOutput;

class AudioSpectrum final : public VideoStream
{
public:
    AudioSpectrum(QString fileName, QSize resolution,
        QObject *parent = nullptr);

    void seek(std::chrono::milliseconds targetTime) override;

private:
    void handleStatusChanged(QMediaPlayer::MediaStatus status);
    void handleAudioBuffer(const QAudioBuffer &buffer);
    void appendSample(float sample);
    void publishSpectrum();
    void resetAnalysis();
    void failLoading();

    QMediaPlayer *mPlayer{ };
    QAudioOutput *mAudioOutput{ };
    QAudioBufferOutput *mBufferOutput{ };
    int mAmplitudeCount{ };
    int mSignalSize{ };
    int mSpectrumHopSize{ };
    KissFFT mFft;
    std::vector<float> mSignal;
    std::vector<float> mOrderedSignal;
    std::chrono::milliseconds mTargetTime{ };
    int mWriteIndex{ };
    int mSampleCount{ };
    int mSamplesSinceSpectrum{ };
};

#endif // defined(MULTIMEDIA_ENABLED)
