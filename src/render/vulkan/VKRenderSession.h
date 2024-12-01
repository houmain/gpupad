#pragma once

#include "render/RenderSessionBase.h"
#include "VKContext.h"

class VKRenderer;
class VKShareSync;
class ScriptSession;

class VKRenderSession final : public RenderSessionBase
{
public:
    explicit VKRenderSession(RendererPtr renderer);
    ~VKRenderSession();

    void render() override;
    void finish() override;
    void release() override;

private:
    struct CommandQueue;

    VKRenderer &renderer();
    void createCommandQueue();
    void reuseUnmodifiedItems();
    void executeCommandQueue();
    void downloadModifiedResources();
    void outputTimerQueries();

    std::shared_ptr<VKShareSync> mShareSync;
    std::unique_ptr<CommandQueue> mCommandQueue;
    std::unique_ptr<CommandQueue> mPrevCommandQueue;
    MessagePtrSet mTimerMessages;
};
