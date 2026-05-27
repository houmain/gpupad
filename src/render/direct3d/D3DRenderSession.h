#pragma once

#include "render/RenderSessionBase.h"
#include "D3DContext.h"
#if defined(_WIN32)

struct D3DContext;
class D3DDevice;
struct ID3D12CommandAllocator;
struct ID3D12Fence;

class D3DRenderSession final : public RenderSessionBase
{
public:
    struct CommandQueue;

    explicit D3DRenderSession(RendererPtr renderer);
    ~D3DRenderSession();

    void render() override;
    void finish() override;
    void release() override;
    quint64 getBufferHandle(ItemId itemId) override;
    std::vector<Duration> resetTimeQueries(size_t count) override;
    std::shared_ptr<void> beginTimeQuery(size_t index) override;

private:
    D3DDevice &d3dDevice();
    void createCommandQueue();

    std::unique_ptr<CommandQueue> mCommandQueue;
    std::unique_ptr<CommandQueue> mPrevCommandQueue;
    ComPtr<ID3D12CommandAllocator> mCommandAllocator;
    ComPtr<ID3D12Fence> mFence;
    uint64_t mFenceValue{};
    ComPtr<ID3D12QueryHeap> mTimeQueryHeap;
    ComPtr<ID3D12Resource> mTimeQueryResolveBuffer;
};

#endif // _WIN32
