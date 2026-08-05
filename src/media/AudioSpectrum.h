#pragma once

#if defined(MULTIMEDIA_ENABLED)

#  include "KissFFT.h"
#  include "VideoStream.h"
#  include <QMediaPlayer>
#  include <array>

class QAudioBuffer;
class QAudioBufferOutput;
class QAudioOutput;

class AudioSpectrum final : public VideoStream
{
public:
    AudioSpectrum(QString fileName, bool flipVertically,
        QObject *parent = nullptr);

    void seek(std::chrono::milliseconds targetTime) override;

private:
    static constexpr int SignalSize = 2048;
    static constexpr int SpectrumHopSize = SignalSize / 2;

    void handleStatusChanged(QMediaPlayer::MediaStatus status);
    void handleAudioBuffer(const QAudioBuffer &buffer);
    void appendSample(float sample);
    void publishSpectrum();
    void resetAnalysis();
    void failLoading();

    QMediaPlayer *mPlayer{ };
    QAudioOutput *mAudioOutput{ };
    QAudioBufferOutput *mBufferOutput{ };
    KissFFT mFft{ SignalSize, KissFFT::WindowType::hann };
    std::array<float, SignalSize> mSignal{ };
    std::array<float, SignalSize> mOrderedSignal{ };
    std::chrono::milliseconds mTargetTime{ };
    int mWriteIndex{ };
    int mSampleCount{ };
    int mSamplesSinceSpectrum{ };
};

#endif // defined(MULTIMEDIA_ENABLED)
