
#include "RenderSessionBase.h"
#include "opengl/GLRenderSession.h"

std::unique_ptr<RenderSessionBase> RenderSessionBase::create(RenderAPI api)
{
    return std::make_unique<GLRenderSession>();
}
