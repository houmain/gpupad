#pragma once

#include "render/RenderSessionBase.h"

class VKRenderer;
class ScriptSession;

class VKRenderSession final : public RenderSessionBase
{
public:
    explicit VKRenderSession(VKRenderer &renderer);
    ~VKRenderSession();

    void render() override;
    void finish() override;
    void release() override;

private:
    struct CommandQueue;

    void createCommandQueue();
    void reuseUnmodifiedItems();
    void executeCommandQueue();
    void downloadModifiedResources();
    void outputTimerQueries();

    VKRenderer &mRenderer;
    std::unique_ptr<CommandQueue> mCommandQueue;
    std::unique_ptr<CommandQueue> mPrevCommandQueue;
    MessagePtrSet mTimerMessages;
};
