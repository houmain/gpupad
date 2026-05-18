#pragma once
#if defined(VULKAN_ENABLED)

#  include "render/RenderSessionBase.h"
#  include "VKContext.h"

class VKRenderer;
class VKShareSync;

class VKRenderSession final : public RenderSessionBase
{
public:
    struct CommandQueue;

    explicit VKRenderSession(RendererPtr renderer);
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

#endif // defined(VULKAN_ENABLED)