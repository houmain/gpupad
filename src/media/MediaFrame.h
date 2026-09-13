#pragma once

#include <QByteArray>
#include <QImage>
#include <QMetaType>
#include "MediaSource.h"

class MediaFrame
{
    Q_GADGET
    Q_PROPERTY(QImage image READ image CONSTANT)
    Q_PROPERTY(QByteArray audioSamples READ audioSamples CONSTANT)
    Q_PROPERTY(qint64 videoStartTime READ videoStartTime CONSTANT)
    Q_PROPERTY(qint64 videoEndTime READ videoEndTime CONSTANT)
    Q_PROPERTY(qint64 audioStartTime READ audioStartTime CONSTANT)
    Q_PROPERTY(int audioSampleRate READ audioSampleRate CONSTANT)
    Q_PROPERTY(QString errorString READ errorString CONSTANT)
    Q_PROPERTY(bool isFinish READ isFinish CONSTANT)

public:
    MediaFrame() = default;
    explicit MediaFrame(QString errorString)
        : mErrorString(std::move(errorString))
    {
    }

    MediaFrame(QImage image, QByteArray audioSamples,
        qint64 videoStartTime, qint64 videoEndTime, qint64 audioStartTime,
        int audioSampleRate, bool isFinish)
        : mImage(std::move(image))
        , mAudioSamples(std::move(audioSamples))
        , mVideoStartTime(videoStartTime)
        , mVideoEndTime(videoEndTime)
        , mAudioStartTime(audioStartTime)
        , mAudioSampleRate(audioSampleRate)
        , mIsFinish(isFinish)
    {
    }

    const QImage &image() const { return mImage; }
    const QByteArray &audioSamples() const { return mAudioSamples; }
    qint64 videoStartTime() const { return mVideoStartTime; }
    qint64 videoEndTime() const { return mVideoEndTime; }
    qint64 audioStartTime() const { return mAudioStartTime; }
    int audioSampleRate() const { return mAudioSampleRate; }
    const QString &errorString() const { return mErrorString; }
    bool isFinish() const { return mIsFinish; }

private:
    QImage mImage;
    QByteArray mAudioSamples;
    qint64 mVideoStartTime{};
    qint64 mVideoEndTime{};
    qint64 mAudioStartTime{};
    int mAudioSampleRate{};
    QString mErrorString;
    bool mIsFinish{};
};

Q_DECLARE_METATYPE(MediaFrame)
