
#include "RenderSessionBase.h"
#include "opengl/GLRenderSession.h"
#include "vulkan/VKRenderSession.h"
#include "vulkan/VKRenderer.h"

std::unique_ptr<RenderSessionBase> RenderSessionBase::create(Renderer &renderer)
{
    switch (renderer.api()) {
        case RenderAPI::OpenGL: return std::make_unique<GLRenderSession>();
        case RenderAPI::Vulkan: return std::make_unique<VKRenderSession>(
            static_cast<VKRenderer&>(renderer));
    }
    return { };
}
