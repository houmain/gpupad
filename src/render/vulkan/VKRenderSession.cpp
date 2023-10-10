
#include "VKRenderSession.h"
#include "VKRenderer.h"

VKRenderSession::VKRenderSession(VKRenderer &renderer)
    : mRenderer(renderer)
{
}

VKRenderSession::~VKRenderSession() = default;

void VKRenderSession::render()
{
    Q_ASSERT(mRenderer.device());
}

void VKRenderSession::finish()
{
    RenderSessionBase::finish();
}

void VKRenderSession::release()
{
}
