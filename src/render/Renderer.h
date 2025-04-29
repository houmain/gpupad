#pragma once

#include <memory>

enum class RenderAPI : int {
    OpenGL,
    Vulkan,
};

using RendererPtr = std::shared_ptr<class Renderer>;
class QThread;
class RenderTask;

class Renderer
{
public:
    explicit Renderer(RenderAPI api) : mApi(api) { }

    virtual ~Renderer() = default;
    virtual QThread *renderThread() = 0;

    RenderAPI api() const { return mApi; }

private:
    friend class RenderTask;
    virtual void render(RenderTask *task) = 0;
    virtual void release(RenderTask *task) = 0;

    RenderAPI mApi;
};
