#pragma once

#if defined(_WIN32)

#include "D3DContext.h"
#include "render/TextureBase.h"

class D3DBuffer;

class D3DTexture : public TextureBase
{
public:
    D3DTexture(const Texture &texture, D3DRenderSession &renderSession);
    D3DTexture(const Buffer &buffer, D3DBuffer *textureBuffer,
        Texture::Format format, D3DRenderSession &renderSession);

    void boundAsSampler() { }
    void boundAsImage() { }
    ID3D12Resource *resource() { return mResource.Get(); }
    void prepareShaderResourceView(D3DContext &context);
    void prepareUnorderedAccessView(D3DContext &context);
    void prepareRenderTargetView(D3DContext &context);
    void prepareDepthStencilView(D3DContext &context);
    D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc() const;
    D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc() const;
    D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc() const;
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc() const;
    bool clear(D3DContext &context, std::array<double, 4> color, double depth,
        int stencil);
    bool copy(D3DContext &context, D3DTexture &source);
    bool swap(D3DTexture &other);
    bool updateMipmaps(D3DContext &context);
    bool deviceCopyModified() const { return mDeviceCopyModified; }
    bool download(D3DContext &context);
    ShareHandle getSharedMemoryHandle() const;

private:
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

    void createAndUpload(D3DContext &context);
    bool upload(D3DContext &context);
    void resourceBarrier(D3DContext &context, D3D12_RESOURCE_STATES state);

    D3DBuffer *mTextureBuffer{};
    bool mCreated{};
    ComPtr<ID3D12Resource> mResource;
    D3D12_RESOURCE_STATES mCurrentState{};
    HANDLE mShareHandle{};
};

#endif // _WIN32
