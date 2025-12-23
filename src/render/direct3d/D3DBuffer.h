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
    void clear(D3DContext &context);
    void copy(D3DContext &context, D3DBuffer &source);
    bool swap(D3DBuffer &other);
    void beginDownload(D3DContext &context, bool checkModification);
    bool finishDownload();
    void prepareVertexBuffer(D3DContext &context);
    void prepareIndexBuffer(D3DContext &context);
    void prepareConstantBufferView(D3DContext &context);
    D3D12_GPU_VIRTUAL_ADDRESS getDeviceAddress();
    
private:
    void createBuffer(D3DContext &context);
    void upload(D3DContext &context);
    void updateReadOnlyBuffer(D3DContext &context);
    void updateReadWriteBuffer(D3DContext &context);
    void resourceBarrier(D3DContext &context, D3D12_RESOURCE_STATES state);

    ComPtr<ID3D12Resource> mResource;
    D3D12_RESOURCE_STATES mCurrentState{};
    ComPtr<ID3D12Resource> mDownloadBuffer;
    bool mDownloading{ };
    bool mCheckModification{ };
};

#endif // _WIN32
