#include "RenderTask.h"

RenderTask::RenderTask(RendererPtr renderer, QObject *parent)
    : QObject(parent)
    , mRenderer(std::move(renderer))
{
    moveToThread(mRenderer->renderThread());
}

RenderTask::~RenderTask()
{
    Q_ASSERT(!mRenderer);
}

void RenderTask::releaseResources()
{
    if (mRenderer)
        mRenderer->release(this);
    mRenderer.reset();
}

void RenderTask::update()
{
    if (!mRenderer)
        return;

    if (!std::exchange(mUpdating, true)) {
        auto itemsChanged = false;
        auto evaluationType = EvaluationType::Reset;
        Q_EMIT preparing(itemsChanged, evaluationType);

        prepare(itemsChanged, evaluationType);
        mRenderer->render(this);
    } else {
        mInvalidated = true;
    }
}

void RenderTask::handleRendered()
{
    finish();
    mUpdating = false;

    Q_EMIT updated();

    // restart when update was called in the meantime
    if (std::exchange(mInvalidated, false))
        update();
}
