
#include "VKRenderSession.h"
#include "VKRenderer.h"

VKRenderSession::VKRenderSession(VKRenderer &renderer)
    : mRenderer(renderer)
{
}

VKRenderSession::~VKRenderSession() = default;

void VKRenderSession::prepare(bool itemsChanged, EvaluationType evaluationType)
{
}

void VKRenderSession::configure()
{
}

void VKRenderSession::configured()
{
}

void VKRenderSession::render()
{
    Q_ASSERT(mRenderer.device());
}

void VKRenderSession::finish()
{
}

void VKRenderSession::release()
{
}

QSet<ItemId> VKRenderSession::usedItems() const
{
    return { };
}

bool VKRenderSession::usesMouseState() const
{
    return { };
}

bool VKRenderSession::usesKeyboardState() const
{
    return { };
}
