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
    switch (desc.ViewDimension) {

    case D3D12_SRV_DIMENSION_TEXTURE1D:
        desc.Texture1D = { .MipLevels = ~UINT{ 0 } };
        break;
    case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
        desc.Texture1DArray = { .MipLevels = ~UINT{ 0 } };
        break;
    case D3D12_SRV_DIMENSION_TEXTURE2D:
        desc.Texture2D = { .MipLevels = ~UINT{ 0 } };
        break;
    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
        desc.Texture2DArray = { .MipLevels = ~UINT{ 0 } };
        break;
    case D3D12_SRV_DIMENSION_TEXTURE2DMS:
    case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY: break;
    case D3D12_SRV_DIMENSION_TEXTURE3D:
        desc.Texture3D = { .MipLevels = ~UINT{ 0 } };
        break;
    case D3D12_SRV_DIMENSION_TEXTURECUBE:
        desc.TextureCube = { .MipLevels = ~UINT{ 0 } };
        break;
    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
        desc.TextureCubeArray = { .MipLevels = ~UINT{ 0 } };
        break;
    default: Q_ASSERT(!"not handled");
    }
    return desc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC D3DTexture::unorderedAccessViewDesc() const
{
    auto desc = D3D12_UNORDERED_ACCESS_VIEW_DESC{
        .Format = toDXGIFormat(mFormat),
        .ViewDimension = toUAVDimension(mTarget, mSamples),
    };
    switch (desc.ViewDimension) {
    case D3D12_UAV_DIMENSION_TEXTURE1D:
    case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
    case D3D12_UAV_DIMENSION_TEXTURE2D:
    case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
    case D3D12_UAV_DIMENSION_TEXTURE2DMS:
    case D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY: break;
    case D3D12_UAV_DIMENSION_TEXTURE3D:
        desc.Texture3D = { .WSize = ~UINT{ 0 } };
        break;
    default: Q_ASSERT(!"not handled");
    }
    return desc;
}

D3D12_RENDER_TARGET_VIEW_DESC D3DTexture::renderTargetViewDesc() const
{
    auto desc = D3D12_RENDER_TARGET_VIEW_DESC{
        .Format = toDXGIFormat(mFormat),
        .ViewDimension = toRTVDimension(mTarget, mSamples),
    };
    // TODO: select array slice
    switch (desc.ViewDimension) {
    case D3D12_RTV_DIMENSION_BUFFER:    desc.Buffer = {}; break;
    case D3D12_RTV_DIMENSION_TEXTURE1D: desc.Texture1D = {}; break;
    case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:
        desc.Texture1DArray = {
            .ArraySize = static_cast<UINT>(-1),
        };
        break;
    case D3D12_RTV_DIMENSION_TEXTURE2D: desc.Texture2D = {}; break;
    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
        desc.Texture2DArray = {
            .ArraySize = static_cast<UINT>(-1),
        };
        break;
    case D3D12_RTV_DIMENSION_TEXTURE2DMS: desc.Texture2DMS = {}; break;
    case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
        desc.Texture2DMSArray = {
            .ArraySize = static_cast<UINT>(-1),
        };
        break;
    case D3D12_RTV_DIMENSION_TEXTURE3D:
        desc.Texture3D = {
            .WSize = static_cast<UINT>(-1),
        };
        break;
    }
    return desc;
}

D3D12_DEPTH_STENCIL_VIEW_DESC D3DTexture::depthStencilViewDesc() const
{
    auto desc = D3D12_DEPTH_STENCIL_VIEW_DESC{
        .Format = toDXGIFormat(mFormat),
        .ViewDimension = toDSVDimension(mTarget, mSamples),
    };
    // TODO: select array slice
    switch (desc.ViewDimension) {
    case D3D12_DSV_DIMENSION_TEXTURE1D: desc.Texture1D = {}; break;
    case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:
        desc.Texture1DArray = {
            .ArraySize = static_cast<UINT>(-1),
        };
        break;
    case D3D12_DSV_DIMENSION_TEXTURE2D: desc.Texture2D = {}; break;
    case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
        desc.Texture2DArray = {
            .ArraySize = static_cast<UINT>(-1),
        };
        break;
    case D3D12_DSV_DIMENSION_TEXTURE2DMS: desc.Texture2DMS = {}; break;
    case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
        desc.Texture2DMSArray = {
            .ArraySize = static_cast<UINT>(-1),
        };
        break;
    }
    return desc;
}

void D3DTexture::prepareShaderResourceView(D3DContext &context,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor)
{
    reload(false);
    create(context);
    upload(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    const auto srvDesc = shaderResourceViewDesc();
    context.device.CreateShaderResourceView(resource(), &srvDesc, descriptor);
}

void D3DTexture::prepareUnorderedAccessView(D3DContext &context,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor)
{
    reload(true);
    create(context);
    upload(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    const auto uavDesc = unorderedAccessViewDesc();
    context.device.CreateUnorderedAccessView(resource(), nullptr, &uavDesc,
        descriptor);

    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;
}

void D3DTexture::prepareRenderTargetView(D3DContext &context)
{
    reload(false);
    create(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;
}

void D3DTexture::prepareDepthStencilView(D3DContext &context)
{
    reload(false);
    create(context);
    resourceBarrier(context, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;
}

bool D3DTexture::clear(D3DContext &context, std::array<double, 4> color,
    double depth, int stencil)
{
    reload(true);
    create(context);
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
        if (!desc.ViewDimension)
            return false;

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
        if (!desc.ViewDimension)
            return false;

        prepareRenderTargetView(context);
        context.renderTargetHelper.ClearRenderTargetView(
            context.graphicsCommandList.Get(), resource(), &desc, clearColor, 0,
            nullptr);
    }
    return true;
}

bool D3DTexture::copy(D3DContext &context, D3DTexture &source)
{
    resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_DEST);
    source.resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_SOURCE);
    context.graphicsCommandList->CopyResource(resource(), source.resource());
    return true;
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
    if (!resource())
        return false;

    if (std::exchange(mMipmapsInvalidated, false) && levels() > 1) {

        resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_DEST);

        const auto transition = CD3DX12_RESOURCE_BARRIER::Transition(resource(),
            mCurrentState, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
        context.graphicsCommandList->ResourceBarrier(1, &transition);

        // TODO: dummy implementation without stretching
        for (auto i = 1; i < levels(); ++i) {
            const auto source =
                CD3DX12_TEXTURE_COPY_LOCATION(resource(), i - 1);
            const auto dest = CD3DX12_TEXTURE_COPY_LOCATION(resource(), i);
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

void D3DTexture::create(D3DContext &context)
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

    const auto faceSlices = depth() * (isCubemapTarget(target()) ? 6 : 1);
    const auto resourceDesc = D3D12_RESOURCE_DESC{
        .Dimension = toResourceDimension(target()),
        .Alignment = 0,
        .Width = static_cast<UINT64>(width()),
        .Height = static_cast<UINT>(height()),
        .DepthOrArraySize = static_cast<UINT16>(faceSlices * layers()),
        .MipLevels = static_cast<UINT16>(levels()),
        .Format = dxgiFormat,
        .SampleDesc = toDXGISampleDesc(samples()),
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = flags,
    };
    const auto heapProperties =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    const auto heapFlags = D3D12_HEAP_FLAG_SHARED;
    if (FAILED(context.device.CreateCommittedResource(&heapProperties,
            heapFlags, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&mResource)))) {
        mMessages +=
            MessageList::insert(mItemId, MessageType::CreatingTextureFailed);
        return;
    }

    AssertIfFailed(context.device.CreateSharedHandle(resource(), nullptr,
        GENERIC_ALL, nullptr, &mShareHandle));
}

ComPtr<ID3D12Resource> D3DTexture::createStagingBuffer(D3DContext &context,
    D3D12_HEAP_TYPE type, uint64_t size)
{
    const auto stagingHeapProperties = CD3DX12_HEAP_PROPERTIES(type);
    const auto stagingBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
    const auto initialState = (type == D3D12_HEAP_TYPE_UPLOAD
            ? D3D12_RESOURCE_STATE_COPY_SOURCE
            : D3D12_RESOURCE_STATE_COPY_DEST);
    auto stagingBuffer = ComPtr<ID3D12Resource>();
    AssertIfFailed(context.device.CreateCommittedResource(
        &stagingHeapProperties, D3D12_HEAP_FLAG_NONE, &stagingBufferDesc,
        initialState, nullptr, IID_PPV_ARGS(&stagingBuffer)));

    context.stagingBuffers.push_back(stagingBuffer);
    return stagingBuffer;
}

void D3DTexture::upload(D3DContext &context)
{
    if (!mSystemCopyModified)
        return;

    if (!mResource || samples() > 1)
        return;

    const auto texDesc = mResource->GetDesc();
    const auto numSubresources = texDesc.MipLevels * layers();
    auto stagingBufferSize = uint64_t{};
    context.device.GetCopyableFootprints(&texDesc, 0, numSubresources, 0,
        nullptr, nullptr, nullptr, &stagingBufferSize);
    const auto stagingBuffer =
        createStagingBuffer(context, D3D12_HEAP_TYPE_UPLOAD, stagingBufferSize);

    resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_DEST);

    auto subresourceData = std::vector<D3D12_SUBRESOURCE_DATA>();
    for (auto level = 0; level < texDesc.MipLevels; ++level)
        for (auto layer = 0; layer < layers(); ++layer) {
            subresourceData.push_back({
                .pData = mData.getData(level, layer, 0),
                .RowPitch = mData.getLevelStride(level),
                .SlicePitch = mData.getImageSize(level),
            });
        }

    UpdateSubresources(context.graphicsCommandList.Get(), resource(),
        stagingBuffer.Get(), 0, 0, static_cast<UINT>(subresourceData.size()),
        subresourceData.data());

    mSystemCopyModified = mDeviceCopyModified = false;
}

void D3DTexture::beginDownload(D3DContext &context)
{
    if (!mDeviceCopyModified)
        return;

    if (mData.isNull() || !mResource)
        return;

    if (samples() > 1)
        return;

    const auto texDesc = mResource->GetDesc();
    const auto numSubresources = texDesc.MipLevels * layers();
    auto layouts = std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>();
    const auto format = toDXGIFormat(mFormat);
    for (auto level = 0; level < texDesc.MipLevels; ++level)
        for (auto layer = 0; layer < layers(); ++layer)
            layouts.push_back({
                .Offset = mData.getOffset(level, layer, 0),
                .Footprint = {
                    .Format = format,
                    .Width = static_cast<UINT>(mData.getLevelWidth(level)),
                    .Height = static_cast<UINT>(mData.getLevelHeight(level)),
                    .Depth = static_cast<UINT>(mData.getLevelDepth(level)),
                    .RowPitch = static_cast<UINT>(mData.getLevelStride(level)),
                },
            });

    const auto stagingBufferSize = mData.getDataSize();
    const auto stagingBuffer = createStagingBuffer(context,
        D3D12_HEAP_TYPE_READBACK, stagingBufferSize);

    resourceBarrier(context, D3D12_RESOURCE_STATE_COPY_SOURCE);

    for (auto i = 0; i < numSubresources; ++i) {
        auto source = CD3DX12_TEXTURE_COPY_LOCATION(resource(), i);
        auto dest =
            CD3DX12_TEXTURE_COPY_LOCATION(stagingBuffer.Get(), layouts[i]);
        context.graphicsCommandList->CopyTextureRegion(&dest, 0, 0, 0, &source,
            nullptr);
    }

    Q_ASSERT(!mDownloadBuffer);
    mDownloadBuffer = stagingBuffer;
    mSystemCopyModified = mDeviceCopyModified = false;
}

bool D3DTexture::finishDownload()
{
    if (!mDownloadBuffer)
        return false;

    auto mappedData = std::add_pointer_t<void>();
    auto readRange = D3D12_RANGE{ 0, 0 };
    AssertIfFailed(mDownloadBuffer->Map(0, &readRange, &mappedData));
    std::memcpy(mData.getWriteonlyData(0, 0, 0), mappedData,
        mData.getDataSize());
    mDownloadBuffer->Unmap(0, nullptr);
    mDownloadBuffer.Reset();
    return true;
}

void D3DTexture::resourceBarrier(D3DContext &context,
    D3D12_RESOURCE_STATES state)
{
    if (!mResource)
        return;

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
