#pragma once

#include <memory>
#include "session/ItemEnums.h"

using RendererPtr = std::shared_ptr<class Renderer>;
class QThread;
class RenderTask;

class Renderer
{
public:
    using Type = ItemEnums::Renderer;
    explicit Renderer(Type type) : mType(type) { }

    virtual ~Renderer() = default;
    virtual QThread *renderThread() = 0;

    Type type() const { return mType; }
    bool failed() const { return mFailed; }
    virtual void finish() = 0;

protected:
    void setFailed() { mFailed = true; }

private:
    friend class RenderTask;
    virtual void render(RenderTask *task) = 0;
    virtual void release(RenderTask *task) = 0;

    const Type mType;
    bool mFailed{};
};
