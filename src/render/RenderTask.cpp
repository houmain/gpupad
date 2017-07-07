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

void RenderTask::update(bool itemsChanged, bool manualEvaluation)
{
    if (!std::exchange(mUpdating, true)) {
        prepare(itemsChanged, manualEvaluation);
        Singletons::renderer().render(this);
    }
}

void RenderTask::handleRendered()
{
    finish();
    mUpdating = false;
    emit updated();
}
