#include "RenderTask.h"

RenderTask::RenderTask(RendererPtr renderer, QObject *parent)
    : QObject(parent)
    , mRenderer(std::move(renderer))
{
}

RenderTask::~RenderTask()
{
    Q_ASSERT(!mRenderer);
}

void RenderTask::releaseResources()
{
    mItemsChanged = false;
    mPendingEvaluation.reset();
    if (mRenderer) {
        mRenderer->release(this);
        mRenderer.reset();
    }
}

void RenderTask::update(bool itemsChanged, EvaluationType evaluationType)
{
    if (!mRenderer)
        return;
    if (!std::exchange(mUpdating, true)) {
        prepare(itemsChanged, evaluationType);
        mRenderer->render(this);
    } else {
        mItemsChanged |= itemsChanged;
        if (evaluationType != EvaluationType::Steady)
            mPendingEvaluation = std::max(evaluationType,
                mPendingEvaluation.value_or(EvaluationType::Steady));
    }
}

void RenderTask::handleRendered()
{
    finish();
    mUpdating = false;

    Q_EMIT updated();

    // restart when items were changed in the meantime
    if (mItemsChanged || mPendingEvaluation.has_value())
        update(std::exchange(mItemsChanged, false),
            std::exchange(mPendingEvaluation, {})
                .value_or(EvaluationType::Steady));
}
