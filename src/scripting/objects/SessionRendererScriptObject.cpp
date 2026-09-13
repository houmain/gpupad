#include "SessionRendererScriptObject.h"
#include "FileCache.h"
#include "InputState.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "media/MediaFrameWriter.h"
#include "media/MediaManager.h"
#include "render/RenderSessionBase.h"
#include "session/SessionModel.h"
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
    struct FrameRequest
    {
        double time{ };
        int frameIndex{ };
        uint64_t soundSampleBase{ };
        int soundFrameCount{ };
        qint64 videoStartTime{ };
        qint64 videoEndTime{ };
        qint64 audioStartTime{ };
        bool isFinish{ };
    };
} // namespace

class SessionRendererScriptObject::State
{
public:
    State(SessionRendererScriptObject &object, const QVariantMap &options)
        : mObject(object)
        , mTextureId(options.value("textureId").toInt())
        , mHasVideo(mTextureId != 0)
        , mHasAudio(options.value("audio", true).toBool())
        , mSampleRate(std::clamp(
              Singletons::sessionModel().sessionItem().audioSampleRate,
              8'000, 384'000))
    {
        if (!mHasVideo && !mHasAudio) {
            mStartupError =
                mObject.tr("The session renderer needs video or audio output.");
            return;
        }
        if (mHasVideo
            && !Singletons::sessionModel().findItem<Texture>(mTextureId)) {
            mStartupError = mObject.tr("The selected texture does not exist.");
            return;
        }

        mPreviousEvaluationMode =
            Singletons::synchronizeLogic().evaluationMode();
        mPreviousTime = Singletons::inputState().time();
        mPreviousFrameIndex = Singletons::inputState().frameIndex();
        mOwnsEvaluationState = true;

        auto &synchronize = Singletons::synchronizeLogic();
        synchronize.setEvaluationMode(EvaluationMode::Paused);
        synchronize.finishEvaluation();
        Singletons::fileCache().updateFromEditors();

        const auto renderer = Singletons::sessionRenderer();
        if (!renderer || renderer->failed()) {
            mStartupError = mObject.tr("The session renderer is unavailable.");
            return;
        }
        mRenderSession = RenderSessionBase::create(renderer);
        if (!mRenderSession) {
            mStartupError = mObject.tr("Could not create a session renderer.");
            return;
        }

        auto &mediaManager = Singletons::mediaManager();
        QObject::connect(&mediaManager, &MediaManager::audioSourcesReady,
            &mObject, [this] {
                if (mClosed)
                    return;
                mWaitingForAudioSources = false;
                if (mRequestPending)
                    startRequest();
            });
        mWaitingForAudioSources = !mediaManager.prepareAudioSources(true);

        auto &frameWriter = mRenderSession->mediaFrameWriter();
        frameWriter.setRecording(mTextureId);
        QObject::connect(&frameWriter, &MediaFrameWriter::frameReady, &mObject,
            [this](MediaFrame frame) {
                mRenderedFrame = std::move(frame);
                mFrameReceived = true;
            });
        QObject::connect(mRenderSession.get(), &RenderTask::preparing, &mObject,
            [this](bool &itemsChanged, EvaluationType &evaluationType) {
                itemsChanged = (mRenderedFrameCount == 0);
                evaluationType = (mRenderedFrameCount == 0
                        ? EvaluationType::Reset
                        : EvaluationType::Automatic);
            });
        QObject::connect(mRenderSession.get(), &RenderTask::updated, &mObject,
            [this] {
                QTimer::singleShot(0, &mObject, [this] { rendered(); });
            });

    }

    ~State()
    {
        mDestroying = true;
        cleanup();
    }

    void requestFrame(const QVariantMap &request)
    {
        if (mClosed)
            return emitError(mObject.tr("The session renderer is finished."));
        if (mRequestPending || mRendering) {
            return emitError(
                mObject.tr("A frame request is already being processed."),
                false);
        }

        mRequest = {
            .time = request.value("time").toDouble(),
            .frameIndex = request.value("frameIndex").toInt(),
            .soundSampleBase = request.value("soundSampleBase").toULongLong(),
            .soundFrameCount = request.value("soundFrameCount").toInt(),
            .videoStartTime = request.value("videoStartTime").toLongLong(),
            .videoEndTime = request.value("videoEndTime").toLongLong(),
            .audioStartTime = request.value("audioStartTime").toLongLong(),
            .isFinish = request.value("isFinish").toBool(),
        };
        if (!std::isfinite(mRequest.time) || mRequest.soundFrameCount < 0
            || mRequest.videoEndTime < mRequest.videoStartTime) {
            return emitError(mObject.tr("The frame request is invalid."));
        }

        mRequestPending = true;
        if (!mWaitingForAudioSources || !mStartupError.isEmpty())
            startRequest();
    }

    void close()
    {
        if (mClosed && mCleanedUp)
            return;
        mClosed = true;
        mRequestPending = false;
        cleanup();
    }

private:
    void startRequest()
    {
        if (mClosed || !mRequestPending || mRendering)
            return;
        if (!mStartupError.isEmpty())
            return emitError(mStartupError);
        if (!mRenderSession)
            return emitError(
                mObject.tr("The session renderer is unavailable."));

        mRenderedFrame = { };
        mFrameReceived = false;

        auto &input = Singletons::inputState();
        input.setTime(mRequest.time);
        input.setFrameIndex(mRequest.frameIndex);
        Singletons::mediaManager().seek(mRequest.time);
        mRenderSession->mediaFrameWriter().beginFrame({
            .audioSampleBase = mRequest.soundSampleBase,
            .audioFrameCount =
                mHasAudio ? mRequest.soundFrameCount : 0,
            .videoStartTime = mRequest.videoStartTime,
            .videoEndTime = mRequest.videoEndTime,
            .audioStartTime = mRequest.audioStartTime,
            .audioSampleRate = mSampleRate,
            .isFinish = mRequest.isFinish,
        });
        mRendering = true;
        mRenderSession->update();
    }

    void rendered()
    {
        mRendering = false;
        if (mClosed || !mRequestPending)
            return;
        if (!mFrameReceived)
            return emitError(mObject.tr(
                "The session renderer did not produce a media frame."));

        ++mRenderedFrameCount;
        mRequestPending = false;
        auto frame = std::move(mRenderedFrame);
        if (!frame.errorString().isEmpty() || frame.isFinish()) {
            mClosed = true;
            scheduleCleanup();
        }
        Q_EMIT mObject.frameReady(frame);
    }

    void emitError(QString error, bool close = true)
    {
        mRequestPending = false;
        if (close) {
            mClosed = true;
            scheduleCleanup();
        }
        Q_EMIT mObject.frameReady(MediaFrame(std::move(error)));
    }

    void scheduleCleanup()
    {
        if (mCleanupScheduled || mDestroying)
            return;
        mCleanupScheduled = true;
        QTimer::singleShot(0, &mObject, [this] {
            mCleanupScheduled = false;
            cleanup();
        });
    }

    void cleanup()
    {
        if (mCleanedUp)
            return;
        mCleanedUp = true;
        if (mRenderSession) {
            QObject::disconnect(mRenderSession.get(), nullptr, &mObject,
                nullptr);
            mRenderSession->renderer().finish();
            mRenderSession.reset();
        }
        if (mOwnsEvaluationState) {
            auto &input = Singletons::inputState();
            input.setTime(mPreviousTime);
            input.setFrameIndex(mPreviousFrameIndex);
            Singletons::mediaManager().seek(mPreviousTime);
            Singletons::synchronizeLogic().setEvaluationMode(
                mPreviousEvaluationMode);
            mOwnsEvaluationState = false;
        }
    }

    SessionRendererScriptObject &mObject;
    const ItemId mTextureId{ };
    const bool mHasVideo{ };
    const bool mHasAudio{ };
    const int mSampleRate{ };
    std::unique_ptr<RenderSessionBase> mRenderSession;
    FrameRequest mRequest;
    MediaFrame mRenderedFrame;
    QString mStartupError;
    int mRenderedFrameCount{ };
    EvaluationMode mPreviousEvaluationMode{ };
    double mPreviousTime{ };
    int mPreviousFrameIndex{ };
    bool mRequestPending{ };
    bool mRendering{ };
    bool mFrameReceived{ };
    bool mWaitingForAudioSources{ };
    bool mClosed{ };
    bool mDestroying{ };
    bool mCleanedUp{ };
    bool mCleanupScheduled{ };
    bool mOwnsEvaluationState{ };
};

SessionRendererScriptObject::SessionRendererScriptObject(
    const QVariantMap &options, QObject *parent)
    : QObject(parent)
    , mState(std::make_unique<State>(*this, options))
{
}

SessionRendererScriptObject::~SessionRendererScriptObject() = default;

void SessionRendererScriptObject::requestFrame(const QVariantMap &request)
{
    mState->requestFrame(request);
}

void SessionRendererScriptObject::close()
{
    mState->close();
}
