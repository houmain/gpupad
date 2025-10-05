#pragma once

#if defined(_WIN32)

#include "render/RenderSessionBase.h"
#include "D3DContext.h"

class D3DRenderer;
class D3DShareSync;
struct ID3D12CommandAllocator;
struct ID3D12Fence;

class D3DRenderSession final : public RenderSessionBase
{
public:
    struct CommandQueue;

    D3DRenderSession(RendererPtr renderer, const QString &basePath);
    ~D3DRenderSession();

    void render() override;
    void finish() override;
    void release() override;
    quint64 getBufferHandle(ItemId itemId) override;

private:
    D3DRenderer &renderer();
    void createCommandQueue();

    std::shared_ptr<D3DShareSync> mShareSync;
    std::unique_ptr<CommandQueue> mCommandQueue;
    std::unique_ptr<CommandQueue> mPrevCommandQueue;
    ComPtr<ID3D12CommandAllocator> mCommandAllocator;
    ComPtr<ID3D12Fence> mFence;
    uint64_t mFenceValue{};
};

#endif // _WIN32
