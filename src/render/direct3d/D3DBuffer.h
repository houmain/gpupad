#pragma once

#if defined(_WIN32)

#include "D3DContext.h"
#include "render/BufferBase.h"

class D3DBuffer : public BufferBase
{
public:
    D3DBuffer(const Buffer &buffer, D3DRenderSession &renderSession);
    explicit D3DBuffer(int size);

    ID3D12Resource *resource() { return mResource.Get(); }
    UINT alignedSize() const { return static_cast<UINT>((mSize + 255) & ~255); }
    const ID3D12Resource &resource() const { return *mResource.Get(); }

    void reload();
    void initialize(D3DContext &context);
    void clear(D3DContext &context);
    void copy(D3DContext &context, D3DBuffer &source);
    bool swap(D3DBuffer &other);
    void upload(D3DContext &context);
    void upload(D3DContext &context, const void *data, size_t size);
    void beginDownload(D3DContext &context, bool checkModification);
    bool finishDownload();
    void prepareCopySource(D3DContext &context);
    void prepareVertexBuffer(D3DContext &context);
    void prepareIndexBuffer(D3DContext &context);
    void prepareConstantBufferView(D3DContext &context,
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor);
    void prepareUnorderedAccessView(D3DContext &context,
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor);
    D3D12_GPU_VIRTUAL_ADDRESS getDeviceAddress();

private:
    void createBuffer(D3DContext &context);
    ComPtr<ID3D12Resource> createStagingBuffer(D3DContext &context,
        D3D12_HEAP_TYPE type);
    void updateReadOnlyBuffer(D3DContext &context);
    void updateReadWriteBuffer(D3DContext &context);
    void resourceBarrier(D3DContext &context, D3D12_RESOURCE_STATES state);

    ComPtr<ID3D12Resource> mResource;
    ComPtr<ID3D12Resource> mDownloadBuffer;
    ComPtr<ID3D12DescriptorHeap> mShaderVisibleDescHeap;
    ComPtr<ID3D12DescriptorHeap> mNonShaderVisibleDescHeap;
    D3D12_RESOURCE_STATES mCurrentState{};
    bool mCheckModification{};
};

#endif // _WIN32
