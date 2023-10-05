#include "RenderTask.h"
#include "Singletons.h"
#include "Renderer.h"

RenderTask::RenderTask(QObject *parent)
    : QObject(parent)
{
}

RenderTask::~RenderTask()
{
    Q_ASSERT(!mInitialized || mReleased);
}

void RenderTask::releaseResources()
{
    Q_ASSERT(!mReleased);
    mReleased = true;
    mItemsChanged = false;
    mPendingEvaluation.reset();
    Singletons::renderer().release(this);
}

void RenderTask::update(bool itemsChanged, EvaluationType evaluationType)
{
    Q_ASSERT(!mReleased);
    if (!std::exchange(mUpdating, true)) {
        auto &renderer = Singletons::renderer();
        if (!mInitialized) {
            initialize(renderer.api());
            mInitialized = true;
        }
        prepare(itemsChanged, evaluationType);
        renderer.render(this);
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
        update(std::exchange(mItemsChanged, false),
            std::exchange(mPendingEvaluation, { })
                .value_or(EvaluationType::Steady));
}
