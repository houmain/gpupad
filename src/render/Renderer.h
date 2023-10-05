#pragma once

#include "RendererBase.h"

class Renderer
{
public:
    explicit Renderer(RenderAPI api = RenderAPI::OpenGL) 
        : mImpl(RendererBase::create(api))
        , mApi(api)
    {
    }

    RenderAPI api() const 
    {
        return mApi;
    }

    void render(RenderTask *task)
    {
        mImpl->render(task);
    }

    void release(RenderTask *task)
    {
        mImpl->release(task);
    }

private:
    RenderAPI mApi;
    std::unique_ptr<RendererBase> mImpl;
};
