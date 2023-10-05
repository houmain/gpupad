#pragma once

#include <memory>

enum class RenderAPI : uint8_t
{
    OpenGL,
};

class RenderTask;

class RendererBase
{
public:
    static std::unique_ptr<RendererBase> create(RenderAPI api);

    virtual ~RendererBase() = default;
    virtual void render(RenderTask *task) = 0;
    virtual void release(RenderTask *task) = 0;
};
