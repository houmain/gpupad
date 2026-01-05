#pragma once

#include "render/RenderSessionBase.h"
#include "VKContext.h"

class VKRenderer;
class VKShareSync;

class VKRenderSession final : public RenderSessionBase
{
public:
    struct CommandQueue;

    VKRenderSession(RendererPtr renderer, const QString &basePath);
    ~VKRenderSession();

    void render() override;
    void finish() override;
    void release() override;
    quint64 getBufferHandle(ItemId itemId) override;
    std::vector<Duration> resetTimeQueries(size_t count) override;
    std::shared_ptr<void> beginTimeQuery(size_t index) override;

private:
    VKRenderer &renderer();
    void createCommandQueue();

    std::shared_ptr<VKShareSync> mShareSync;
    std::unique_ptr<CommandQueue> mCommandQueue;
    std::unique_ptr<CommandQueue> mPrevCommandQueue;
    KDGpu::TimestampQueryRecorder mTimestampQueries;
};
