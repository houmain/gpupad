#include "VKTexture.h"
#include "SynchronizeLogic.h"
#include "VKBuffer.h"
#include <QScopeGuard>
#include <cmath>

namespace {
    KDGpu::TextureType getKDTextureType(const TextureKind &kind)
    {
        if (kind.cubeMap)
            return KDGpu::TextureType::TextureTypeCube;

        switch (kind.dimensions) {
        case 1: return KDGpu::TextureType::TextureType1D;
        case 2: return KDGpu::TextureType::TextureType2D;
        case 3: return KDGpu::TextureType::TextureType3D;
        }
        return {};
    }

    KDGpu::ViewType getKDViewType(const TextureKind &kind)
    {
        using KD = KDGpu::ViewType;
        if (kind.cubeMap)
            return (kind.array ? KD::ViewTypeCubeArray : KD::ViewTypeCube);

        switch (kind.dimensions) {
        case 1: return (kind.array ? KD::ViewType1DArray : KD::ViewType1D);
        case 2: return (kind.array ? KD::ViewType2DArray : KD::ViewType2D);
        case 3: return KD::ViewType3D;
        }
        return {};
    }
} // namespace

VKTexture::VKTexture(const Texture &texture, VKRenderSession &renderSession)
    : TextureBase(texture, renderSession)
{
    mUsage =
        KDGpu::TextureUsageFlags{ KDGpu::TextureUsageFlagBits::TransferSrcBit
            | KDGpu::TextureUsageFlagBits::TransferDstBit };
}

VKTexture::VKTexture(const Buffer &buffer, VKBuffer *textureBuffer,
    Texture::Format format, VKRenderSession &renderSession)
    : TextureBase(buffer, format, renderSession)
    , mTextureBuffer(textureBuffer)
{
}

void VKTexture::addUsage(KDGpu::TextureUsageFlags usage)
{
    // TODO: not ideal to update usage of already created texture
    if (mTexture.isValid() && (mUsage & usage) != usage)
        mTexture = {};
    mUsage |= usage;
}

KDGpu::TextureView &VKTexture::getView(int level, int layer,
    KDGpu::Format format)
{
    auto &view = mTextureViews[ViewOptions{
        .level = level,
        .layer = layer,
        .format = format,
    }];
    if (!view.isValid()) {
        auto options = KDGpu::TextureViewOptions{
            .viewType = getKDViewType(mKind),
            .format = format,
            .range = { .aspectMask = aspectMask() },
        };
        if (level >= 0) {
            options.range.baseMipLevel = level;
            options.range.levelCount = 1;
        }
        if (layer >= 0) {
            options.range.baseArrayLayer = layer;
            options.range.layerCount = 1;
        }
        view = mTexture.createView(options);
    }
    return view;
}

bool VKTexture::prepareSampledImage(VKContext &context)
{
    reload(false);
    createAndUpload(context);

    memoryBarrier(*context.commandRecorder,
        KDGpu::TextureLayout::ShaderReadOnlyOptimal,
        KDGpu::AccessFlagBit::MemoryReadBit,
        KDGpu::PipelineStageFlagBit::AllCommandsBit);

    return mTexture.isValid();
}

bool VKTexture::prepareStorageImage(VKContext &context)
{
    reload(false);
    createAndUpload(context);
    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;

    memoryBarrier(*context.commandRecorder, KDGpu::TextureLayout::General,
        KDGpu::AccessFlagBit::ShaderStorageWriteBit
            | KDGpu::AccessFlagBit::ShaderStorageReadBit,
        KDGpu::PipelineStageFlagBit::AllCommandsBit);

    return mTexture.isValid();
}

bool VKTexture::prepareAttachment(VKContext &context)
{
    reload(true);
    createAndUpload(context);
    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;

    const auto layout = (mKind.depth || mKind.stencil
            ? KDGpu::TextureLayout::DepthStencilAttachmentOptimal
            : KDGpu::TextureLayout::ColorAttachmentOptimal);

    memoryBarrier(*context.commandRecorder, layout,
        KDGpu::AccessFlagBit::MemoryReadBit,
        KDGpu::PipelineStageFlagBit::AllCommandsBit);

    return mTexture.isValid();
}

bool VKTexture::prepareTransferSource(VKContext &context)
{
    reload(false);
    createAndUpload(context);

    memoryBarrier(*context.commandRecorder,
        KDGpu::TextureLayout::TransferSrcOptimal,
        KDGpu::AccessFlagBit::MemoryReadBit,
        KDGpu::PipelineStageFlagBit::TransferBit);

    return mTexture.isValid();
}

bool VKTexture::clear(VKContext &context, std::array<double, 4> color,
    double depth, int stencil)
{
    reload(true);
    createAndUpload(context);
    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;

    if (!mTexture.isValid())
        return false;

    memoryBarrier(*context.commandRecorder, KDGpu::TextureLayout::General,
        KDGpu::AccessFlagBit::TransferWriteBit,
        KDGpu::PipelineStageFlagBit::TransferBit);

    if (mKind.depth || mKind.stencil) {
        context.commandRecorder->clearDepthStencilTexture({
            .texture = mTexture,
            .layout = mCurrentLayout,
            .depthClearValue = static_cast<float>(depth),
            .stencilClearValue = static_cast<uint32_t>(stencil),
            .ranges = { KDGpu::TextureSubresourceRange{
                .aspectMask = aspectMask() } },
        });
    } else {
        // cannot clear compressed formats
        if (getTextureDataType(mFormat) == TextureDataType::Compressed)
            return false;

        const auto sampleType = getTextureSampleType(mFormat);
        transformClearColor(color, sampleType);

        context.commandRecorder->clearColorTexture({
            .texture = mTexture,
            .layout = mCurrentLayout,
            .clearValue = KDGpu::ColorClearValue{ static_cast<float>(color[0]),
                static_cast<float>(color[1]), static_cast<float>(color[2]),
                static_cast<float>(color[3]) },
            .ranges = { KDGpu::TextureSubresourceRange{
                .aspectMask = aspectMask() } },
        });
    }
    return true;
}

bool VKTexture::copy(VKContext &context, VKTexture &source)
{
    context.commandRecorder->copyTextureToTexture({ 
        .srcTexture = source.texture(),
        .srcLayout = mCurrentLayout,
        .dstTexture = texture(),
        .dstLayout = KDGpu::TextureLayout::General,
        .regions = { KDGpu::TextureCopyRegion{ 
            .srcSubresource{ .aspectMask = source.aspectMask() },
            .dstSubresource{ .aspectMask = aspectMask() },
            .extent = {
                static_cast<uint32_t>(mWidth),
                static_cast<uint32_t>(mHeight),
                static_cast<uint32_t>(mDepth),
            }, }, }, 
        });

    mCurrentLayout = KDGpu::TextureLayout::General;
    return true;
}

bool VKTexture::swap(VKTexture &other)
{
    if (!TextureBase::swap(other))
        return false;

    std::swap(mTextureBuffer, other.mTextureBuffer);
    std::swap(mCreated, other.mCreated);
    std::swap(mUsage, other.mUsage);
    std::swap(mKtxTexture, other.mKtxTexture);
    std::swap(mTexture, other.mTexture);
    std::swap(mTextureViews, other.mTextureViews);
    std::swap(mCurrentLayout, other.mCurrentLayout);
    std::swap(mCurrentAccessMask, other.mCurrentAccessMask);
    std::swap(mCurrentStage, other.mCurrentStage);
    return true;
}

bool VKTexture::updateMipmaps(VKContext &context)
{
    if (std::exchange(mMipmapsInvalidated, false) && levels() > 1) {
        const auto options = KDGpu::GenerateMipMapsOptions{
            .texture = mTexture,
            .layout = mCurrentLayout,
            .newLayout = KDGpu::TextureLayout::TransferSrcOptimal,
            .extent = {
                .width = static_cast<uint32_t>(mWidth),
                .height = static_cast<uint32_t>(mHeight),
                .depth = static_cast<uint32_t>(mDepth),
            },
            .mipLevels = static_cast<uint32_t>(levels()),
            .layerCount = static_cast<uint32_t>(layers()),
        };
        context.commandRecorder->generateMipMaps(options);
        mCurrentLayout = options.newLayout;
    }
    return true;
}

void VKTexture::reset(KDGpu::Device &device)
{
    if (std::exchange(mCreated, false)) {
        if (mKtxTexture.vkDestroyImage) {
            auto vkDevice = static_cast<KDGpu::VulkanDevice *>(
                device.graphicsApi()->resourceManager()->getDevice(device));
            ktxVulkanTexture_Destruct(&mKtxTexture, vkDevice->device, nullptr);
        }
        mTexture = {};
        mTextureViews.clear();
    }
}

void VKTexture::createAndUpload(VKContext &context)
{
    if (mSystemCopyModified)
        reset(context.device);

    if (std::exchange(mCreated, true))
        return;

    auto textureOptions = KDGpu::TextureOptions{
        .type = getKDTextureType(mKind),
        .format = toKDGpu(mFormat),
        .extent = { 
            static_cast<uint32_t>(mWidth), 
            static_cast<uint32_t>(mHeight),
            static_cast<uint32_t>(mDepth),
        },
        .mipLevels = static_cast<uint32_t>(levels()),
        .arrayLayers = static_cast<uint32_t>(mLayers),
        .samples = getKDSampleCount(mSamples),
        .usage = mUsage,
        .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
    };
    const auto isWritten = (mUsage
        & (KDGpu::TextureUsageFlagBits::ColorAttachmentBit
            | KDGpu::TextureUsageFlagBits::DepthStencilAttachmentBit
            | KDGpu::TextureUsageFlagBits::StorageBit));

    if (isWritten || mSamples > 1) {
        mTexture = {};
    } else {
        mCurrentLayout = KDGpu::TextureLayout::TransferDstOptimal;
        if (mData.uploadVK(&context.ktxDeviceInfo, &mKtxTexture,
                static_cast<VkImageUsageFlags>(mUsage.toInt()),
                static_cast<VkImageLayout>(mCurrentLayout))) {
            auto vkApi = static_cast<KDGpu::VulkanGraphicsApi *>(
                context.device.graphicsApi());
            mTexture = vkApi->createTextureFromExistingVkImage(context.device,
                textureOptions, mKtxTexture.image);
        } else {
            mMessages +=
                MessageList::insert(mItemId, MessageType::UploadingImageFailed);
        }
    }

    if (!mTexture.isValid()) {
        // check if format is supported
        const auto properties =
            context.device.adapter()->formatProperties(textureOptions.format);
        if (!properties.optimalTilingFeatures) {
            mMessages += MessageList::insert(mItemId,
                MessageType::UnsupportedTextureFormat);
            textureOptions.format = KDGpu::Format::R8_UNORM;
        }

#if defined(KDGPU_PLATFORM_WIN32)
        textureOptions.externalMemoryHandleType =
            KDGpu::ExternalMemoryHandleTypeFlagBits::OpaqueWin32;
#else
        textureOptions.externalMemoryHandleType =
            KDGpu::ExternalMemoryHandleTypeFlagBits::OpaqueFD;
#endif
        mTexture = context.device.createTexture(textureOptions);
        mCurrentLayout = textureOptions.initialLayout;
    }
    mSystemCopyModified = mDeviceCopyModified = false;
}

void VKTexture::beginDownload(VKContext &context)
{
    if (!mDeviceCopyModified || !mTexture.isValid() || mData.isNull())
        return;

    Q_ASSERT(!mDownloadBuffer.isValid());
    mDownloadBuffer = context.device.createBuffer(KDGpu::BufferOptions{
        .size = static_cast<size_t>(mData.getDataSize()),
        .usage = KDGpu::BufferUsageFlagBits::TransferDstBit
            | KDGpu::BufferUsageFlagBits::TransferSrcBit,
        .memoryUsage = KDGpu::MemoryUsage::CpuOnly,
    });

    if (samples() > 1) {
        Q_ASSERT(levels() == 1);
        mResolveTexture = context.device.createTexture(KDGpu::TextureOptions{
            .type = KDGpu::TextureType::TextureType2D,
            .format = toKDGpu(format()),
            .extent = { static_cast<uint32_t>(width()),
                static_cast<uint32_t>(height()), static_cast<uint32_t>(1) },
            .mipLevels = 1,
            .arrayLayers = static_cast<uint32_t>(layers()),
            .samples = KDGpu::SampleCountFlagBits::Samples1Bit,
            .tiling = KDGpu::TextureTiling::Optimal,
            .usage = KDGpu::TextureUsageFlagBits::TransferDstBit
                | KDGpu::TextureUsageFlagBits::TransferSrcBit,
            .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
        });
        Q_ASSERT(mResolveTexture.isValid());
    }

    const auto range =
        KDGpu::TextureSubresourceRange{ .aspectMask = aspectMask() };

    prepareTransferSource(context);

    auto copySource = KDGpu::Handle<KDGpu::Texture_t>(texture());

    if (mResolveTexture.isValid()) {
        context.commandRecorder->textureMemoryBarrier({
            .srcStages = KDGpu::PipelineStageFlagBit::TransferBit,
            .srcMask = KDGpu::AccessFlagBit::None,
            .dstStages = KDGpu::PipelineStageFlagBit::TransferBit,
            .dstMask = KDGpu::AccessFlagBit::TransferWriteBit,
            .oldLayout = KDGpu::TextureLayout::Undefined,
            .newLayout = KDGpu::TextureLayout::TransferDstOptimal,
            .texture = mResolveTexture,
            .range = range,
        });

        context.commandRecorder->resolveTexture({ 
                    .srcTexture = texture(),
                    .srcLayout = KDGpu::TextureLayout::TransferSrcOptimal,
                    .dstTexture = mResolveTexture,
                    .dstLayout = KDGpu::TextureLayout::TransferDstOptimal,
                    .regions = { { .extent = {
                                       .width = static_cast<uint32_t>(width()),
                                       .height = static_cast<uint32_t>(height()),
                                       .depth = 1,
                                   }, } }, 
                  });

        context.commandRecorder->textureMemoryBarrier({
            .srcStages = KDGpu::PipelineStageFlagBit::TransferBit,
            .srcMask = KDGpu::AccessFlagBit::TransferWriteBit,
            .dstStages = KDGpu::PipelineStageFlagBit::TransferBit,
            .dstMask = KDGpu::AccessFlagBit::TransferReadBit,
            .oldLayout = KDGpu::TextureLayout::TransferDstOptimal,
            .newLayout = KDGpu::TextureLayout::TransferSrcOptimal,
            .texture = mResolveTexture,
            .range = range,
        });

        copySource = mResolveTexture;
    }

    context.commandRecorder->bufferMemoryBarrier({
        .srcStages = KDGpu::PipelineStageFlagBit::TransferBit,
        .srcMask = KDGpu::AccessFlagBit::None,
        .dstStages = KDGpu::PipelineStageFlagBit::TransferBit,
        .dstMask = KDGpu::AccessFlagBit::TransferWriteBit,
        .buffer = mDownloadBuffer,
    });

    const auto start = mData.getData(0, 0, 0);
    const auto faceSlices = mData.depth() * mData.faces();
    auto regions = std::vector<KDGpu::BufferTextureCopyRegion>();
    for (auto layer = 0; layer < mData.layers(); ++layer)
        for (auto faceSlice = 0; faceSlice < faceSlices; ++faceSlice)
            regions.push_back(KDGpu::BufferTextureCopyRegion{
                .bufferOffset = static_cast<KDGpu::DeviceSize>(
                    std::distance(start, mData.getData(0, layer, faceSlice))),
                .textureSubResource{
                    .aspectMask = aspectMask(),
                    .baseArrayLayer = static_cast<uint32_t>(layer),
                },
                .textureOffset{
                    .x = 0,
                    .y = 0,
                    .z = static_cast<int32_t>(faceSlice),
                },
                .textureExtent{
                    .width = static_cast<uint32_t>(mData.width()),
                    .height = static_cast<uint32_t>(mData.height()),
                    .depth = 1,
                },
            });
    context.commandRecorder->copyTextureToBuffer({
        .srcTexture = copySource,
        .srcTextureLayout = KDGpu::TextureLayout::TransferSrcOptimal,
        .dstBuffer = mDownloadBuffer,
        .regions = regions,
    });

    context.commandRecorder->bufferMemoryBarrier({
        .srcStages = KDGpu::PipelineStageFlagBit::TransferBit,
        .srcMask = KDGpu::AccessFlagBit::TransferWriteBit,
        .dstStages = KDGpu::PipelineStageFlagBit::TransferBit,
        .dstMask = KDGpu::AccessFlagBit::MemoryReadBit,
        .buffer = mDownloadBuffer,
    });

    mSystemCopyModified = mDeviceCopyModified = false;
}

bool VKTexture::finishDownload()
{
    if (!mDownloadBuffer.isValid())
        return false;

    const auto mappedData = mDownloadBuffer.map();
    std::memcpy(mData.getWriteonlyData(0, 0, 0), mappedData,
        mData.getDataSize());
    mDownloadBuffer.unmap();
    mDownloadBuffer = {};
    return true;
}

KDGpu::TextureAspectFlagBits VKTexture::aspectMask() const
{
    const auto depthStencilBits = static_cast<KDGpu::TextureAspectFlagBits>(
        static_cast<uint32_t>(KDGpu::TextureAspectFlagBits::DepthBit)
        | static_cast<uint32_t>(KDGpu::TextureAspectFlagBits::StencilBit));
    return (mKind.stencil && mKind.depth ? depthStencilBits
            : mKind.stencil ? KDGpu::TextureAspectFlagBits::StencilBit
            : mKind.depth   ? KDGpu::TextureAspectFlagBits::DepthBit
                            : KDGpu::TextureAspectFlagBits::ColorBit);
}

void VKTexture::memoryBarrier(KDGpu::CommandRecorder &commandRecorder,
    KDGpu::TextureLayout layout, KDGpu::AccessFlags accessMask,
    KDGpu::PipelineStageFlags stage)
{
    if (!mTexture.isValid())
        return;

    commandRecorder.textureMemoryBarrier(KDGpu::TextureMemoryBarrierOptions{
        .srcStages = mCurrentStage,
        .srcMask = mCurrentAccessMask,
        .dstStages = stage,
        .dstMask = accessMask,
        .oldLayout = mCurrentLayout,
        .newLayout = layout,
        .texture = mTexture,
        .range = { .aspectMask = aspectMask() },
    });
    mCurrentStage = stage;
    mCurrentLayout = layout;
    mCurrentAccessMask = accessMask;
}

ShareHandle VKTexture::getSharedMemoryHandle() const
{
    if (!mTexture.isValid())
        return {};
    const auto memory = mTexture.externalMemoryHandle();
    if (memory.handle.index() == 0)
        return {};
#if defined(KDGPU_PLATFORM_WIN32)
    return {
        ShareHandleType::OPAQUE_WIN32,
        std::get<HANDLE>(memory.handle),
        memory.allocationSize,
        memory.allocationOffset,
        true,
    };
#else
    return {
        ShareHandleType::OPAQUE_FD,
        reinterpret_cast<void *>(intptr_t{ std::get<int>(memory.handle) }),
        memory.allocationSize,
        memory.allocationOffset,
        true,
    };
#endif
}
