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

void RenderTask::update()
{
    ++mInvalidationCount;
    if (mInvalidationCount == 1)
        Singletons::renderer().render(this);
}

void RenderTask::validate()
{
    if (std::exchange(mInvalidationCount, 0) > 1) {
        update();
    }
    else {
        emit updated();
    }
}
