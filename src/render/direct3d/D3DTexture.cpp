#include "D3DTexture.h"
#include "SynchronizeLogic.h"
#include "D3DBuffer.h"
#include <QScopeGuard>
#include <cmath>

namespace {
    D3D12_RESOURCE_DIMENSION toResourceDimension(Texture::Target target)
    {
        using T = Texture::Target;
        switch (target) {
        case T::TargetBuffer:  return D3D12_RESOURCE_DIMENSION_BUFFER;
        case T::Target1D:
        case T::Target1DArray: return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
        default:               return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        case T::Target3D:      return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        }
    }

    D3D12_SRV_DIMENSION toSRVDimension(Texture::Target target, int samples)
    {
        using T = Texture::Target;
        switch (target) {
        case T::TargetBuffer:  return D3D12_SRV_DIMENSION_BUFFER;
        case T::Target1D:      return D3D12_SRV_DIMENSION_TEXTURE1D;
        case T::Target1DArray: return D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
        case T::Target2D:
            return (samples <= 1 ? D3D12_SRV_DIMENSION_TEXTURE2D
                                 : D3D12_SRV_DIMENSION_TEXTURE2DMS);
        case T::Target2DArray:
            return (samples <= 1 ? D3D12_SRV_DIMENSION_TEXTURE2DARRAY
                                 : D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY);
        case T::Target3D:           return D3D12_SRV_DIMENSION_TEXTURE3D;
        case T::TargetCubeMap:      return D3D12_SRV_DIMENSION_TEXTURECUBE;
        case T::TargetCubeMapArray: return D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
        //return D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        default:                    break;
        }
        Q_ASSERT(!"not handled target");
        return D3D12_SRV_DIMENSION_UNKNOWN;
    }

    D3D12_UAV_DIMENSION toUAVDimension(Texture::Target target, int samples)
    {
        using T = Texture::Target;
        switch (target) {
        case T::TargetBuffer:  return D3D12_UAV_DIMENSION_BUFFER;
        case T::Target1D:      return D3D12_UAV_DIMENSION_TEXTURE1D;
        case T::Target1DArray: return D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
        case T::Target2D:
            return (samples <= 1 ? D3D12_UAV_DIMENSION_TEXTURE2D
                                 : D3D12_UAV_DIMENSION_TEXTURE2DMS);
        case T::Target2DArray:
            return (samples <= 1 ? D3D12_UAV_DIMENSION_TEXTURE2DARRAY
                                 : D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY);
        case T::Target3D: return D3D12_UAV_DIMENSION_TEXTURE3D;
        default:          break;
        }
        Q_ASSERT(!"not handled target");
        return D3D12_UAV_DIMENSION_UNKNOWN;
    }

    D3D12_RTV_DIMENSION toRTVDimension(Texture::Target target, int samples)
    {
        using T = Texture::Target;
        switch (target) {
        case T::TargetBuffer:  return D3D12_RTV_DIMENSION_BUFFER;
        case T::Target1D:      return D3D12_RTV_DIMENSION_TEXTURE1D;
        case T::Target1DArray: return D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
        case T::Target2D:
            return (samples <= 1 ? D3D12_RTV_DIMENSION_TEXTURE2D
                                 : D3D12_RTV_DIMENSION_TEXTURE2DMS);
        case T::Target2DArray:
            return (samples <= 1 ? D3D12_RTV_DIMENSION_TEXTURE2DARRAY
                                 : D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY);
        case T::Target3D: return D3D12_RTV_DIMENSION_TEXTURE3D;
        default:          break;
        }
        Q_ASSERT(!"not handled target");
        return D3D12_RTV_DIMENSION_UNKNOWN;
    }

    D3D12_DSV_DIMENSION toDSVDimension(Texture::Target target, int samples)
    {
        using T = Texture::Target;
        switch (target) {
        case T::Target1D:      return D3D12_DSV_DIMENSION_TEXTURE1D;
        case T::Target1DArray: return D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
        case T::Target2D:
            return (samples <= 1 ? D3D12_DSV_DIMENSION_TEXTURE2D
                                 : D3D12_DSV_DIMENSION_TEXTURE2DMS);
        case T::Target2DArray:
            return (samples <= 1 ? D3D12_DSV_DIMENSION_TEXTURE2DARRAY
                                 : D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY);
        default: break;
        }
        Q_ASSERT(!"not handled target");
        return D3D12_DSV_DIMENSION_UNKNOWN;
    }
} // namespace

D3DTexture::D3DTexture(const Texture &texture, D3DRenderSession &renderSession)
    : TextureBase(texture, renderSession)
{
}

D3DTexture::D3DTexture(const Buffer &buffer, D3DBuffer *textureBuffer,
    Texture::Format format, D3DRenderSession &renderSession)
    : TextureBase(buffer, format, renderSession)
    , mTextureBuffer(textureBuffer)
{
}

D3D12_SHADER_RESOURCE_VIEW_DESC D3DTexture::shaderResourceViewDesc() const
{
    auto desc = D3D12_SHADER_RESOURCE_VIEW_DESC{
        .Format = toDXGIFormat(mFormat),
        .ViewDimension = toSRVDimension(mTarget, mSamples),
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    };
    // TODO:
    desc.Texture2D.MostDetailedMip = 0;
    desc.Texture2D.MipLevels = -1;
    return desc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC D3DTexture::unorderedAccessViewDesc() const
{
    auto desc = D3D12_UNORDERED_ACCESS_VIEW_DESC{
        .Format = toDXGIFormat(mFormat),
        .ViewDimension = toUAVDimension(mTarget, mSamples),
    };
    // TODO:
    desc.Texture2D.MipSlice = 0;
    desc.Texture2D.PlaneSlice = 0;
    return desc;
}

D3D12_RENDER_TARGET_VIEW_DESC D3DTexture::renderTargetViewDesc() const
{
    auto desc = D3D12_RENDER_TARGET_VIEW_DESC{
        .Format = toDXGIFormat(mFormat),
        .ViewDimension = toRTVDimension(mTarget, mSamples),
    };
    // TODO: select array slice
    return desc;
}

D3D12_DEPTH_STENCIL_VIEW_DESC D3DTexture::depthStencilViewDesc() const
{
    auto desc = D3D12_DEPTH_STENCIL_VIEW_DESC{
        .Format = toDXGIFormat(mFormat),
        .ViewDimension = toDSVDimension(mTarget, mSamples),
    };
    // TODO: select array slice
    return desc;
}

void D3DTexture::prepareShaderResourceView(D3DContext &context)
{
    reload(false);
    createAndUpload(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
}

void D3DTexture::prepareUnorderedAccessView(D3DContext &context)
{
    reload(true);
    createAndUpload(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;
}

void D3DTexture::prepareRenderTargetView(D3DContext &context)
{
    reload(false);
    createAndUpload(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;
}

void D3DTexture::prepareDepthStencilView(D3DContext &context)
{
    reload(false);
    createAndUpload(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;
}

bool D3DTexture::clear(D3DContext &context, std::array<double, 4> color,
    double depth, int stencil)
{
    reload(true);
    createAndUpload(context);
    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;

    if (!mResource)
        return false;

    if (mKind.depth || mKind.stencil) {
        auto flags = D3D12_CLEAR_FLAGS{};
        if (mKind.depth)
            flags |= D3D12_CLEAR_FLAG_DEPTH;
        if (mKind.stencil)
            flags |= D3D12_CLEAR_FLAG_STENCIL;

        const auto desc = depthStencilViewDesc();
        prepareDepthStencilView(context);
        context.renderTargetHelper.ClearDepthStencilView(
            context.graphicsCommandList.Get(), resource(), &desc, flags,
            static_cast<float>(depth), static_cast<uint8_t>(stencil), 0,
            nullptr);

    } else {
        const auto sampleType = getTextureSampleType(mFormat);
        transformClearColor(color, sampleType);

        float clearColor[4] = {
            static_cast<float>(color[0]),
            static_cast<float>(color[1]),
            static_cast<float>(color[2]),
            static_cast<float>(color[3]),
        };

        const auto desc = renderTargetViewDesc();
        prepareRenderTargetView(context);
        context.renderTargetHelper.ClearRenderTargetView(
            context.graphicsCommandList.Get(), resource(), &desc, clearColor, 0,
            nullptr);
    }
    return true;
}

bool D3DTexture::copy(D3DContext &context, D3DTexture &source)
{
    Q_ASSERT(!"not implemented");
    return false;
}

bool D3DTexture::swap(D3DTexture &other)
{
    if (!TextureBase::swap(other))
        return false;

    std::swap(mTextureBuffer, other.mTextureBuffer);
    std::swap(mCreated, other.mCreated);
    std::swap(mResource, other.mResource);
    std::swap(mCurrentState, other.mCurrentState);
    std::swap(mShareHandle, other.mShareHandle);
    return true;
}

bool D3DTexture::updateMipmaps(D3DContext &context)
{
    if (std::exchange(mMipmapsInvalidated, false) && levels() > 1) {

        resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_DEST);

        const auto transition = CD3DX12_RESOURCE_BARRIER::Transition(resource(),
            mCurrentState, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
        context.graphicsCommandList->ResourceBarrier(1, &transition);

        // TODO: dummy implementation without stretching
        for (auto i = 1; i < levels(); ++i) {
            const auto source =
                CD3DX12_TEXTURE_COPY_LOCATION(mResource.Get(), i - 1);
            const auto dest = CD3DX12_TEXTURE_COPY_LOCATION(mResource.Get(), i);
            const auto sourceRegion = D3D12_BOX{
                .left = 0,
                .top = 0,
                .front = 0,
                .right = static_cast<UINT>(mData.getLevelWidth(i)),
                .bottom = static_cast<UINT>(mData.getLevelHeight(i)),
                .back = 1,
            };
            context.graphicsCommandList->CopyTextureRegion(&dest, 0, 0, 0,
                &source, &sourceRegion);

            const auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
                resource(), mCurrentState, D3D12_RESOURCE_STATE_COPY_SOURCE, i);
            context.graphicsCommandList->ResourceBarrier(1, &transition);
        }
        mCurrentState = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    return true;
}

void D3DTexture::createAndUpload(D3DContext &context)
{
    if (std::exchange(mCreated, true))
        return;

    auto dxgiFormat = toDXGITypelessFormat(format());
    if (!dxgiFormat)
        dxgiFormat = toDXGIFormat(format());
    if (!dxgiFormat) {
        mMessages +=
            MessageList::insert(mItemId, MessageType::UnsupportedTextureFormat);
        return;
    }

    const auto flags = D3D12_RESOURCE_FLAGS{ mKind.depth || mKind.stencil
            ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
            : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
                | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };

    const auto resourceDesc = D3D12_RESOURCE_DESC{
        .Dimension = toResourceDimension(target()),
        .Alignment = 0,
        .Width = static_cast<UINT64>(width()),
        .Height = static_cast<UINT>(height()),
        .DepthOrArraySize = static_cast<UINT16>(depth() * layers()),
        .MipLevels = static_cast<UINT16>(levels()),
        .Format = dxgiFormat,
        .SampleDesc = toDXGISampleDesc(samples()),
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = flags,
    };
    const auto heapProperties =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    const auto heapFlags = D3D12_HEAP_FLAG_SHARED;
    AssertIfFailed(context.device.CreateCommittedResource(&heapProperties,
        heapFlags, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(&mResource)));

    AssertIfFailed(context.device.CreateSharedHandle(mResource.Get(), nullptr,
        GENERIC_ALL, nullptr, &mShareHandle));

    upload(context);
}

bool D3DTexture::upload(D3DContext &context)
{
    if (samples() > 1)
        return false;

    const auto numSubresources = static_cast<UINT>(levels() * layers());
    const auto uploadBufferSize =
        GetRequiredIntermediateSize(mResource.Get(), 0, numSubresources);

    const auto stagingHeapProperties =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto stagingBufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    auto stagingBuffer = ComPtr<ID3D12Resource>();
    AssertIfFailed(context.device.CreateCommittedResource(
        &stagingHeapProperties, D3D12_HEAP_FLAG_NONE, &stagingBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&stagingBuffer)));

    resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_DEST);

    auto subresourceData = std::vector<D3D12_SUBRESOURCE_DATA>();
    for (auto level = 0; level < levels(); ++level)
        for (auto layer = 0; layer < layers(); ++layer)
            subresourceData.push_back({
                .pData = mData.getData(level, layer, 0),
                .RowPitch = mData.getLevelStride(level),
                .SlicePitch = mData.getImageSize(level),
            });
    UpdateSubresources(context.graphicsCommandList.Get(), mResource.Get(),
        stagingBuffer.Get(), 0, 0, numSubresources, subresourceData.data());

    context.stagingBuffers.push_back(std::move(stagingBuffer));

    mSystemCopyModified = mDeviceCopyModified = false;
    return true;
}

void D3DTexture::beginDownload(D3DContext &context)
{
    if (!mDeviceCopyModified)
        return;

    if (mData.isNull() || !mResource)
        return;

    if (samples() > 1)
        return;

    mSystemCopyModified = mDeviceCopyModified = false;
    //mDownloading = true;
}

bool D3DTexture::finishDownload()
{
    if (!mDownloading)
        return false;

    mDownloading = false;
    return true;
}

void D3DTexture::resourceBarrier(D3DContext &context,
    D3D12_RESOURCE_STATES state)
{
    if (state == mCurrentState) {
        if (state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
            const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource());
            context.graphicsCommandList->ResourceBarrier(1, &barrier);
        }
    } else {
        const auto transition = CD3DX12_RESOURCE_BARRIER::Transition(resource(),
            mCurrentState, state);
        context.graphicsCommandList->ResourceBarrier(1, &transition);
        mCurrentState = state;
    }
}

ShareHandle D3DTexture::getSharedMemoryHandle() const
{
    return {
        ShareHandleType::D3D12_RESOURCE,
        mShareHandle,
    };
}
