#pragma once

#include "render/RenderSessionBase.h"
#include "VKContext.h"

class VKRenderer;
class VKShareSync;

class VKRenderSession final : public RenderSessionBase
{
public:
    VKRenderSession(RendererPtr renderer, const QString &basePath);
    ~VKRenderSession();

    void render() override;
    void finish() override;
    void release() override;
    quint64 getTextureHandle(ItemId itemId) override { return 0; }
    quint64 getBufferHandle(ItemId itemId) override;

private:
    struct CommandQueue;

    VKRenderer &renderer();
    void createCommandQueue();
    void buildCommandQueue();
    void reuseUnmodifiedItems();
    void executeCommandQueue();
    void downloadModifiedResources();
    void outputTimerQueries();

    std::shared_ptr<VKShareSync> mShareSync;
    std::unique_ptr<CommandQueue> mCommandQueue;
    std::unique_ptr<CommandQueue> mPrevCommandQueue;
    MessagePtrSet mTimerMessages;
};
