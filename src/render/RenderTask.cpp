#include "RenderTask.h"

RenderTask::RenderTask(QObject *parent)
    : QObject(parent)
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

void RenderTask::update(RendererPtr renderer, bool itemsChanged, EvaluationType evaluationType)
{
    if (!std::exchange(mUpdating, true)) {
        if (mRenderer != renderer) {
            releaseResources();
            if (!renderer || !initialize(*renderer))
                return;
            mRenderer = renderer;
        }
        prepare(itemsChanged, evaluationType);
        mRenderer->render(this);
    }
    else {
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
        update(mRenderer, std::exchange(mItemsChanged, false),
            std::exchange(mPendingEvaluation, { })
                .value_or(EvaluationType::Steady));
}
