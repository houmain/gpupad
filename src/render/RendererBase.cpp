
#include "RendererBase.h"
#include "opengl/GLRenderer.h"

std::unique_ptr<RendererBase> RendererBase::create(RenderAPI api)
{
    return std::make_unique<GLRenderer>();
}
