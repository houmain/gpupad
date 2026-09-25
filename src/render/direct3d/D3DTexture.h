#pragma once
#if defined(D3D_ENABLED)

#  include "D3DContext.h"
#  include "render/TextureBase.h"
#  include <vector>

class D3DBuffer;

class D3DTexture : public TextureBase
{
public:
    D3DTexture(const Texture &texture, D3DRenderSession &renderSession);
    D3DTexture(const Buffer &buffer, D3DBuffer *textureBuffer,
        Texture::Format format, D3DRenderSession &renderSession);
    void setTextureBuffer(D3DBuffer *buffer) { mTextureBuffer = buffer; }
    D3DTexture(TextureData data, int samples, ItemId itemId = 0);
    D3DTexture(D3DTexture &&) noexcept = default;
    D3DTexture &operator=(D3DTexture &&) noexcept = default;
    ~D3DTexture();

    void boundAsSampler() { }
    void boundAsImage() { }

    ID3D12Resource *resource() { return mResource.Get(); }
    void prepareShaderResourceView(D3DContext &context,
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor);
    void prepareUnorderedAccessView(D3DContext &context,
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor);
    void prepareRenderTargetView(D3DContext &context);
    void prepareDepthStencilView(D3DContext &context);
    D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc() const;
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc() const;
    bool clear(D3DContext &context, std::array<double, 4> color, double depth,
        int stencil);
    bool copy(D3DContext &context, D3DTexture &source);
    bool swap(D3DTexture &other);
    bool updateMipmaps(D3DContext &context);
    bool deviceCopyModified() const { return mDeviceCopyModified; }
    void prepareExternalRead(D3DContext &context);
    void beginDownload(D3DContext &context);
    bool finishDownload();
    ShareHandle getShareHandle() const;

private:
    struct DownloadSubresource
    {
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        UINT rowCount;
        UINT64 rowSize;
        int level;
        int layer;
        int faceSlice;
    };

    struct ViewOptions
    {
        int level;
        int layer;
        DXGI_FORMAT format;

        friend bool operator<(const ViewOptions &a, const ViewOptions &b)
        {
            return std::tie(a.level, a.layer, a.format)
                < std::tie(b.level, b.layer, b.format);
        }
    };

    ComPtr<ID3D12Resource> createStagingBuffer(D3DContext &context,
        D3D12_HEAP_TYPE type, uint64_t size);
    void create(D3DContext &context);
    void upload(D3DContext &context);
    D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc() const;
    D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc() const;
    void resourceBarrier(D3DContext &context, D3D12_RESOURCE_STATES state);

    D3DBuffer *mTextureBuffer{};
    bool mCreated{};
    ComPtr<ID3D12Resource> mResource;
    ComPtr<ID3D12Resource> mDownloadBuffer;
    std::vector<DownloadSubresource> mDownloadSubresources;
    D3D12_RESOURCE_STATES mCurrentState{};
    D3D12_CLEAR_VALUE mClearValue{};
};

#endif // D3D_ENABLED
