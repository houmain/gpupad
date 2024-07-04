#include "VKTexture.h"
#include "VKBuffer.h"
#include "SynchronizeLogic.h"
#include "EvaluatedPropertyCache.h"
#include <cmath>

namespace
{
    KDGpu::TextureType getKDTextureType(const TextureKind &kind)
    {
        if (kind.cubeMap)
            return KDGpu::TextureType::TextureTypeCube;

        switch (kind.dimensions) {
            case 1: return KDGpu::TextureType::TextureType1D;
            case 2: return KDGpu::TextureType::TextureType2D;
            case 3: return KDGpu::TextureType::TextureType3D;
        }
        return { };
    }

    KDGpu::ViewType getKDViewType(const TextureKind &kind)
    {
        if (kind.cubeMap)
            return (kind.array ? KDGpu::ViewType::ViewTypeCubeArray :
                                 KDGpu::ViewType::ViewTypeCube);

        switch (kind.dimensions) {
            case 1: return (kind.array ? KDGpu::ViewType::ViewType1DArray :
                                         KDGpu::ViewType::ViewType1D);
            case 2: return (kind.array ? KDGpu::ViewType::ViewType2DArray :
                                         KDGpu::ViewType::ViewType2D);
            case 3: return KDGpu::ViewType::ViewType3D;
        }
        return { };
    }

    class TransferTexture
    {
    private:
        KDGpu::Texture mTexture;

    public:
        TransferTexture(KDGpu::Device &device, const VKTexture &texture)
        {
            mTexture = device.createTexture({
                .type = getKDTextureType(texture.kind()),
                .format = toKDGpu(texture.format()),
                .extent = {
                    static_cast<uint32_t>(texture.width()), 
                    static_cast<uint32_t>(texture.height()),
                    static_cast<uint32_t>(texture.depth())
                },
                .mipLevels = 1,
                .arrayLayers = static_cast<uint32_t>(texture.layers()),
                .samples = KDGpu::SampleCountFlagBits::Samples1Bit,
                .tiling = KDGpu::TextureTiling::Linear,
                .usage = KDGpu::TextureUsageFlagBits::TransferDstBit | 
                         KDGpu::TextureUsageFlagBits::TransferSrcBit,
                .memoryUsage = KDGpu::MemoryUsage::CpuOnly
            });    
        }

        bool download(VKContext &context, VKTexture &texture)
        {
            if (!mTexture.isValid())
                return false;

            texture.prepareDownload(context);
            
            context.commandRecorder->textureMemoryBarrier({
                .srcStages = KDGpu::PipelineStageFlagBit::TransferBit,
                .srcMask = KDGpu::AccessFlagBit::None,
                .dstStages = KDGpu::PipelineStageFlagBit::TransferBit,
                .dstMask = KDGpu::AccessFlagBit::TransferWriteBit,
                .oldLayout = KDGpu::TextureLayout::Undefined,
                .newLayout = KDGpu::TextureLayout::TransferDstOptimal,
                .texture = mTexture,
                .range = { .aspectMask = texture.aspectMask() }
            });

            context.commandRecorder->copyTextureToTexture({
                .srcTexture = texture.texture(),
                .srcLayout = KDGpu::TextureLayout::TransferSrcOptimal,
                .dstTexture = mTexture,
                .dstLayout = KDGpu::TextureLayout::TransferDstOptimal,
                .regions = {{
                    .extent = { 
                        .width = static_cast<uint32_t>(texture.width()), 
                        .height = static_cast<uint32_t>(texture.height()), 
                        .depth = 1
                    }
                }}
            });

            context.commandRecorder->textureMemoryBarrier({
                .srcStages = KDGpu::PipelineStageFlagBit::TransferBit,
                .srcMask = KDGpu::AccessFlagBit::TransferWriteBit,
                .dstStages = KDGpu::PipelineStageFlagBit::TransferBit,
                .dstMask = KDGpu::AccessFlagBit::MemoryReadBit,
                .oldLayout = KDGpu::TextureLayout::TransferDstOptimal,
                .newLayout = KDGpu::TextureLayout::General,
                .texture = mTexture,
                .range = { .aspectMask = texture.aspectMask() }
            });
            return true;
        }

        std::byte *map()
        {
            const auto subresourceLayout = mTexture.getSubresourceLayout();
            auto data = static_cast<std::byte*>(mTexture.map());
            data += subresourceLayout.offset;
            return data;
        }

        void unmap()
        {
            mTexture.unmap();
        }
    };
} // namespace

VKTexture::VKTexture(const Texture &texture, ScriptEngine &scriptEngine)
    : TextureBase(texture, scriptEngine)
{
    mUsage = KDGpu::TextureUsageFlags{
        KDGpu::TextureUsageFlagBits::TransferSrcBit |
        KDGpu::TextureUsageFlagBits::TransferDstBit |
        (mKind.depth || mKind.stencil ? 
            KDGpu::TextureUsageFlagBits::DepthStencilAttachmentBit :
            KDGpu::TextureUsageFlagBits::ColorAttachmentBit |
            KDGpu::TextureUsageFlagBits::SampledBit)
    };
}

VKTexture::VKTexture(const Buffer &buffer, VKBuffer *textureBuffer,
      Texture::Format format, ScriptEngine &scriptEngine)
    : TextureBase(buffer, format, scriptEngine)
    , mTextureBuffer(textureBuffer)
{
}

void VKTexture::addUsage(KDGpu::TextureUsageFlags usage)
{
    mUsage |= usage;
}

bool VKTexture::prepareSampledImage(VKContext &context)
{
    reload(false);
    createAndUpload(context);

    memoryBarrier(*context.commandRecorder, 
        KDGpu::TextureLayout::ShaderReadOnlyOptimal,
        KDGpu::AccessFlagBit::MemoryReadBit, 
        KDGpu::PipelineStageFlagBit::AllGraphicsBit);

    return mTextureView.isValid();
}

bool VKTexture::prepareStorageImage(VKContext &context)
{
    reload(false);
    createAndUpload(context);
    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;

    memoryBarrier(*context.commandRecorder, 
        KDGpu::TextureLayout::General,
        KDGpu::AccessFlagBit::MemoryWriteBit | KDGpu::AccessFlagBit::MemoryReadBit, 
        KDGpu::PipelineStageFlagBit::AllGraphicsBit);

    return mTextureView.isValid();
}

bool VKTexture::prepareAttachment(VKContext &context)
{
    reload(true);
    createAndUpload(context);
    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;

    const auto layout =
        mKind.depth || mKind.stencil ? KDGpu::TextureLayout::DepthStencilAttachmentOptimal :
        //mKind.stencil ? KDGpu::TextureLayout::StencilAttachmentOptimal :
        //mKind.depth ? KDGpu::TextureLayout::DepthAttachmentOptimal :
                      KDGpu::TextureLayout::ColorAttachmentOptimal;

    memoryBarrier(*context.commandRecorder, 
        layout,
        KDGpu::AccessFlagBit::MemoryReadBit, 
        KDGpu::PipelineStageFlagBit::AllGraphicsBit);

    return mTextureView.isValid();
}

bool VKTexture::prepareDownload(VKContext &context)
{
    reload(false);
    createAndUpload(context);

    memoryBarrier(*context.commandRecorder, 
        KDGpu::TextureLayout::TransferSrcOptimal,
        KDGpu::AccessFlagBit::MemoryReadBit, 
        KDGpu::PipelineStageFlagBit::TransferBit);

    return mTextureView.isValid();
}

bool VKTexture::clear(VKContext &context, std::array<double, 4> color, 
    double depth, int stencil)
{
    reload(true);
    createAndUpload(context);
    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;

    if (!mTextureView.isValid())
        return false;

    memoryBarrier(*context.commandRecorder, 
        KDGpu::TextureLayout::General,
        KDGpu::AccessFlagBit::TransferWriteBit, 
        KDGpu::PipelineStageFlagBit::TransferBit);

    auto succeeded = true;
    if (mKind.depth || mKind.stencil) {
        context.commandRecorder->clearDepthStencilTexture({
            .texture = mTexture,
            .layout = mCurrentLayout,
            .depthClearValue = static_cast<float>(depth),
            .stencilClearValue = static_cast<uint32_t>(stencil),
            .ranges = {
                KDGpu::TextureSubresourceRange {
                    .aspectMask = aspectMask()
                }
            }
        });
    }
    else {
        const auto sampleType = getTextureSampleType(mFormat);
        transformClearColor(color, sampleType);

        context.commandRecorder->clearColorTexture({
            .texture = mTexture,
            .layout = mCurrentLayout,
            .clearValue = KDGpu::ColorClearValue{ 
                static_cast<float>(color[0]),
                static_cast<float>(color[1]),
                static_cast<float>(color[2]),
                static_cast<float>(color[3])    
            },
            .ranges = {
                KDGpu::TextureSubresourceRange {
                    .aspectMask = aspectMask()
                }
            }
        });
    }
    return succeeded;
}

bool VKTexture::copy(VKContext &context, VKTexture &source)
{
    context.commandRecorder->copyTextureToTexture({
        .srcTexture = source.texture(),
        .srcLayout = mCurrentLayout,
        .dstTexture = texture(),
        .dstLayout = KDGpu::TextureLayout::General,
        .regions = {
            KDGpu::TextureCopyRegion{ 
                .srcSubresource{ .aspectMask = source.aspectMask() },
                .dstSubresource{ .aspectMask = aspectMask() },
                .extent = {
                    static_cast<uint32_t>(mWidth), 
                    static_cast<uint32_t>(mHeight),
                    static_cast<uint32_t>(mDepth),
                }
            }
        }
    });
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
    std::swap(mTextureView, other.mTextureView);
    std::swap(mCurrentLayout, other.mCurrentLayout);
    std::swap(mCurrentAccessMask, other.mCurrentAccessMask);
    std::swap(mCurrentStage, other.mCurrentStage);
    return true;
}

void VKTexture::reset(KDGpu::Device& device)
{
    if (std::exchange(mCreated, false)) {
        if (mKtxTexture.vkDestroyImage) {
            auto vkDevice = static_cast<KDGpu::VulkanDevice*>(
                device.graphicsApi()->resourceManager()->getDevice(device));
            ktxVulkanTexture_Destruct(&mKtxTexture, vkDevice->device, nullptr);
        }
        mTexture = { };
        mTextureView = { };
    }
}

void VKTexture::createAndUpload(VKContext &context)
{
    if (mSystemCopyModified)
        reset(context.device);

    if (std::exchange(mCreated, true))
      return;

    const auto textureOptions = KDGpu::TextureOptions{
        .type = getKDTextureType(mKind),
        .format = toKDGpu(mFormat),
        .extent = { 
            static_cast<uint32_t>(mWidth), 
            static_cast<uint32_t>(mHeight),
            static_cast<uint32_t>(mDepth),
        },
        .mipLevels = static_cast<uint32_t>(mData.levels()),
        .arrayLayers = static_cast<uint32_t>(mLayers),
        .samples = getKDSampleCount(1),//mSamples),
        .usage = mUsage,
        .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
    };

    if (mKind.depth || mKind.stencil) {
        mTexture = context.device.createTexture(textureOptions);
    }
    else {
        mCurrentLayout = KDGpu::TextureLayout::TransferDstOptimal;

        auto data = mData;
        if (mData.format() != mFormat)
            data = mData.convert(mFormat);

        if (!data.uploadVK(&context.ktxDeviceInfo, &mKtxTexture, 
                static_cast<VkImageUsageFlags>(mUsage.toInt()),
                static_cast<VkImageLayout>(mCurrentLayout))) {
            mMessages += MessageList::insert(mItemId,
                (data.isNull() ? MessageType::UploadingImageFailed :
                                 MessageType::CreatingTextureFailed));
            return;
        }
        auto vkApi = static_cast<KDGpu::VulkanGraphicsApi*>(context.device.graphicsApi());
        mTexture = vkApi->createTextureFromExistingVkImage(context.device, 
            textureOptions, mKtxTexture.image);
    }
    if (mTexture.isValid()) {
        auto options = KDGpu::TextureViewOptions{
            .viewType = getKDViewType(mKind),
            .format = KDGpu::Format::UNDEFINED,
            .range = { .aspectMask = aspectMask() }
        };
        mTextureView = mTexture.createView(options);
    }
    mSystemCopyModified = mDeviceCopyModified = false;
}

bool VKTexture::download(VKContext &context)
{
    if (mKind.depth)
        return false;

    if (!mDeviceCopyModified)
        return false;

    if (!mTexture.isValid())
        return false;

    auto transferTexture = TransferTexture(context.device, *this);

    context.commandRecorder = context.device.createCommandRecorder();
    auto guard = qScopeGuard([&] { context.commandRecorder.reset(); });

    if (!transferTexture.download(context, *this)) {
        mMessages += MessageList::insert(
            mItemId, MessageType::DownloadingImageFailed);
        return false;
    }

    auto commandBuffer = context.commandRecorder->finish();
    context.queue.submit({ .commandBuffers = { commandBuffer} });
    context.queue.waitUntilIdle();

    if (!mData.create(mTarget, mFormat, mWidth, mHeight, mDepth, mLayers))
        return false;

    auto data = transferTexture.map();
    std::memcpy(mData.getWriteonlyData(0, 0, 0), data, mData.getLevelSize(0));
    transferTexture.unmap();

    mSystemCopyModified = mDeviceCopyModified = false;
    return true;
}

KDGpu::TextureAspectFlagBits VKTexture::aspectMask() const
{
    return mKind.stencil ? KDGpu::TextureAspectFlagBits::StencilBit :
           mKind.depth ? KDGpu::TextureAspectFlagBits::DepthBit :
           KDGpu::TextureAspectFlagBits::ColorBit;
}

void VKTexture::memoryBarrier(KDGpu::CommandRecorder &commandRecorder, 
    KDGpu::TextureLayout layout, KDGpu::AccessFlags accessMask, KDGpu::PipelineStageFlags stage) 
{
    if (!mTextureView.isValid())
        return;

    commandRecorder.textureMemoryBarrier(KDGpu::TextureMemoryBarrierOptions{
        .srcStages = mCurrentStage,
        .srcMask = mCurrentAccessMask,
        .dstStages = stage,
        .dstMask = accessMask,
        .oldLayout = mCurrentLayout,
        .newLayout = layout,
        .texture = mTexture,
        .range = { .aspectMask = aspectMask() }
    });
    mCurrentStage = stage;
    mCurrentLayout = layout;
    mCurrentAccessMask = accessMask;
}
