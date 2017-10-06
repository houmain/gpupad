#include "RenderTask.h"
#include "Singletons.h"
#include "Renderer.h"

RenderTask::RenderTask(QObject *parent) : QObject(parent)
{
}

RenderTask::~RenderTask()
{
    Q_ASSERT(mReleased);
}

void RenderTask::releaseResources()
{
    if (!std::exchange(mReleased, true))
        Singletons::renderer().release(this);
}

void RenderTask::update(bool itemsChanged, bool manualEvaluation,
    bool steadyEvaluation)
{
    if (!std::exchange(mUpdating, true)) {
        mReleased = false;
        prepare(itemsChanged, manualEvaluation);
        Singletons::renderer().render(this);
    }
    else {
        mItemsChanged |= itemsChanged;
        mManualEvaluation |= manualEvaluation;
    }
    mSteadyEvaluation = steadyEvaluation;
}

void RenderTask::handleRendered()
{
    finish(mSteadyEvaluation);
    mUpdating = false;

    // restart when items were changed in the meantime
    if (mItemsChanged || mManualEvaluation)
        return update(std::exchange(mItemsChanged, false),
                      std::exchange(mManualEvaluation, false),
                      mSteadyEvaluation);

    emit updated();
}
