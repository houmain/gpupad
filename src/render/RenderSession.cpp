#include "RenderSession.h"

RenderSession::RenderSession(QObject *parent) : RenderTask(parent)
{
}

RenderSession::~RenderSession()
{
    releaseResources();
}

void RenderSession::prepare()
{
    // TODO
}

void RenderSession::render(QOpenGLContext &glContext)
{
    if (!mContext)
        mContext.reset(new GLContext(glContext));

    // TODO
}

void RenderSession::finish()
{
    // TODO
}

void RenderSession::release(QOpenGLContext &glContext)
{
    // TODO
}
