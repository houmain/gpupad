#if defined(MULTIMEDIA_ENABLED)

#  include "VideoPlayer.h"
#  include "TextureData.h"
#  include "FileDialog.h"
#  include <QVideoSink>

namespace {
    const auto MaxQueuedFrames = 4;
    const auto SeekThreshold = std::chrono::seconds(1);
    const auto DecodeSpeed = 4;

    std::chrono::microseconds startTime(const QVideoFrame &frame)
    {
        return std::chrono::microseconds(frame.startTime());
    }

    std::chrono::microseconds endTime(const QVideoFrame &frame)
    {
        return std::chrono::microseconds(frame.endTime());
    }
} // namespace

VideoPlayer::VideoPlayer(QString fileName, QObject *parent)
    : VideoStream(fileName, parent)
{
    mPlayer = new QMediaPlayer(this);
    mSink = new QVideoSink();
    connect(mPlayer, &QMediaPlayer::mediaStatusChanged, this,
        &VideoPlayer::handleStatusChanged);
    connect(mSink, &QVideoSink::videoFrameChanged, this,
        &VideoPlayer::handleFrameDecoded);
    mPlayer->setSource(QUrl::fromLocalFile(fileName));
    mPlayer->setLoops(QMediaPlayer::Once);
    mPlayer->setPlaybackRate(DecodeSpeed);
    mPlayer->setVideoSink(mSink);
}

void VideoPlayer::handleStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::InvalidMedia) {
        mPlayer->deleteLater();
        mPlayer = nullptr;
        Q_EMIT loadingFinished();
    } else if (status == QMediaPlayer::LoadedMedia) {
        mPlayer->play();
    } else if (status == QMediaPlayer::EndOfMedia) {
        mPlayer->setPosition(0);
        ++mLoopCount;
    }
}

void VideoPlayer::handleFrameDecoded(QVideoFrame frame)
{
    if (!width()) {
        mDuration = std::chrono::milliseconds(mPlayer->duration());

        // speedup image sequences from 25 to 60 fps
        if (FileDialog::isSequenceFileName(fileName()))
            mPlaybackSpeed = 2.4;
    }

    if (!frame.isValid()) {
        mPlayer->pause();
        return;
    }

    mDuration = std::max(mDuration, endTime(frame));
    mSeeking = false;

    // offset frame time by loop count
    const auto offset = mDuration * mLoopCount;
    frame.setStartTime(((startTime(frame) + offset) / mPlaybackSpeed).count());
    frame.setEndTime(((endTime(frame) + offset) / mPlaybackSpeed).count());
    mDecodeTime = endTime(frame);

    // queue frames not ending before taget time
    if (mTargetTime < endTime(frame) || mFrameQueue.empty())
        mFrameQueue.push_back(frame);

    // stop decoding when queue is full
    if (mFrameQueue.size() >= MaxQueuedFrames)
        mPlayer->pause();

    // present frame at target time
    if (startTime(frame) <= mTargetTime && endTime(frame) > mTargetTime)
        presentFrame(frame);
}

void VideoPlayer::seek(std::chrono::milliseconds time)
{
    if (!mPlayer || mTargetTime == time || !mDuration.count())
        return;
    mTargetTime = time;

    // present frame at target time
    const auto bestFrame = [&]() -> QVideoFrame {
        auto bestFrame = QVideoFrame{ };
        auto best = int64_t{ };
        for (auto i = 0u; i < mFrameQueue.size(); ++i) {
            const auto &frame = mFrameQueue[i];
            const auto current = std::abs(
                ((startTime(frame) + endTime(frame)) / 2 - time).count());
            if (!bestFrame.isValid() || current < best) {
                bestFrame = frame;
                best = current;
            }
        }
        return bestFrame;
    }();
    if (bestFrame.isValid())
        presentFrame(bestFrame);

    // remove frames before target time
    mFrameQueue.erase(
        std::remove_if(mFrameQueue.begin(), mFrameQueue.end(),
            [&](const QVideoFrame &frame) { return endTime(frame) < time; }),
        mFrameQueue.end());

    // schedule seek when target is not right after decode time
    const auto decodeStartTime =
        mFrameQueue.empty() ? mDecodeTime : startTime(mFrameQueue.front());
    if (!mSeeking
        && (time < decodeStartTime || time > mDecodeTime + SeekThreshold)) {
        mSeeking = true;
        if (time < mDecodeTime)
            mFrameQueue.clear();
        mDecodeTime = time;
        auto seekTime = time * mPlaybackSpeed;
        mLoopCount = seekTime / mDuration;
        seekTime -= mLoopCount * mDuration;
        mPlayer->setPosition(seekTime.count());
    }

    // start decoding
    if (mFrameQueue.size() < MaxQueuedFrames)
        mPlayer->play();
}

#endif // MULTIMEDIA_ENABLED
