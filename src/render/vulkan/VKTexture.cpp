#include "VKTexture.h"
#include "VKBuffer.h"
#include "SynchronizeLogic.h"
#include "EvaluatedPropertyCache.h"
#include <cmath>

namespace
{
    class TransferTexture
    {
    private:
        KDGpu::Texture mTexture;

    public:
        TransferTexture(KDGpu::Device &device, const VKTexture &texture)
        {
            mTexture = device.createTexture({
                .type = KDGpu::TextureType::TextureType2D, // TODO
                .format = toKDGpu(texture.format()),
                .extent = { static_cast<uint32_t>(texture.width()), 
                            static_cast<uint32_t>(texture.height()), 1 },
                .mipLevels = 1,
                .samples = KDGpu::SampleCountFlagBits::Samples1Bit,
                .tiling = KDGpu::TextureTiling::Linear,
                .usage = KDGpu::TextureUsageFlagBits::TransferDstBit | 
                         KDGpu::TextureUsageFlagBits::TransferSrcBit,
                .memoryUsage = KDGpu::MemoryUsage::CpuOnly
            });    
        }

        bool download(VKContext &context, VKTexture &texture)
        {
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
    : mItemId(texture.id)
    , mFileName(texture.fileName)
    , mFlipVertically(texture.flipVertically)
    , mTarget(texture.target)
    , mFormat(texture.format)
    , mSamples(texture.samples)
    , mKind(getKind(texture))
{
    Singletons::evaluatedPropertyCache().evaluateTextureProperties(
        texture, &mWidth, &mHeight, &mDepth, &mLayers, &scriptEngine);

    if (mKind.dimensions < 2)
        mHeight = 1;
    if (mKind.dimensions < 3)
        mDepth = 1;
    if (!mKind.array)
        mLayers = 1;

    mUsedItems += texture.id;
}

bool VKTexture::operator==(const VKTexture &rhs) const
{
    return std::tie(mMessages, mFileName, mFlipVertically, mTarget, mFormat, mWidth, mHeight, mDepth, mLayers, mSamples) ==
           std::tie(rhs.mMessages, rhs.mFileName, rhs.mFlipVertically, rhs.mTarget, rhs.mFormat, rhs.mWidth, rhs.mHeight, rhs.mDepth, rhs.mLayers, rhs.mSamples);
}

bool VKTexture::prepareImageSampler(VKContext &context)
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

bool VKTexture::copy(VKTexture &source)
{
    // TODO:
    return false;
}

bool VKTexture::swap(VKTexture &other)
{
    if (mTarget != other.mTarget || 
        mFormat != other.mFormat || 
        mWidth != other.mWidth || 
        mHeight != other.mHeight || 
        mDepth != other.mDepth || 
        mLayers != other.mLayers || 
        mSamples != other.mSamples)
        return false;

    std::swap(mData, other.mData);
    std::swap(mDataWritten, other.mDataWritten);

    std::swap(mKtxTexture, other.mKtxTexture);
    std::swap(mTexture, other.mTexture);
    std::swap(mTextureView, other.mTextureView);

    std::swap(mCurrentLayout, other.mCurrentLayout);
    std::swap(mCurrentAccessMask, other.mCurrentAccessMask);
    std::swap(mCurrentStage, other.mCurrentStage);

    std::swap(mSystemCopyModified, other.mSystemCopyModified);
    std::swap(mDeviceCopyModified, other.mDeviceCopyModified);
    std::swap(mMipmapsInvalidated, other.mMipmapsInvalidated);
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

    const auto usage = KDGpu::TextureUsageFlags{ 
        KDGpu::TextureUsageFlagBits::TransferSrcBit |
        KDGpu::TextureUsageFlagBits::TransferDstBit |
        (mKind.depth ? 
            KDGpu::TextureUsageFlagBits::DepthStencilAttachmentBit :
            KDGpu::TextureUsageFlagBits::ColorAttachmentBit |
            KDGpu::TextureUsageFlagBits::SampledBit |
            KDGpu::TextureUsageFlagBits::StorageBit)
    };

    const auto textureOptions = KDGpu::TextureOptions{
        .type = KDGpu::TextureType::TextureType2D,
        .format = toKDGpu(mFormat),
        .extent = { 
            static_cast<uint32_t>(mWidth), 
            static_cast<uint32_t>(mHeight),
            static_cast<uint32_t>(mDepth),
        },
        .mipLevels = 1, // TODO
        .samples = KDGpu::SampleCountFlagBits::Samples1Bit, //mSamples,
        .usage = usage,
        .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
    };

    if (mKind.depth || mKind.stencil) {
        mTexture = context.device.createTexture(textureOptions);
    }
    else {
        mCurrentLayout = KDGpu::TextureLayout::TransferDstOptimal;

        if (!mData.uploadVK(&context.ktxDeviceInfo, &mKtxTexture, 
                static_cast<VkImageUsageFlags>(usage.toInt()),
                static_cast<VkImageLayout>(mCurrentLayout))) {
            mMessages += MessageList::insert(mItemId, MessageType::CreatingTextureFailed);
            return;
        }
        auto vkApi = static_cast<KDGpu::VulkanGraphicsApi*>(context.device.graphicsApi());
        mTexture = vkApi->createTextureFromExistingVkImage(context.device, 
            textureOptions, mKtxTexture.image);
    }
    if (mTexture.isValid()) {
        auto options = KDGpu::TextureViewOptions{
            .range = { .aspectMask = aspectMask() }
        };
        mTextureView = mTexture.createView(options);
    }
    mSystemCopyModified = mDeviceCopyModified = false;
}

void VKTexture::reload(bool forWriting)
{
    auto fileData = TextureData{ };
    if (!FileDialog::isEmptyOrUntitled(mFileName))
        if (!Singletons::fileCache().getTexture(mFileName, mFlipVertically, &fileData))
            mMessages += MessageList::insert(mItemId,
                MessageType::LoadingFileFailed, mFileName);

    // reload file as long as targets match
    // ignore dimension mismatch when reading
    // do not ignore when writing (format is ignored)
    const auto hasSameDimensions = [&](const TextureData &data) {
        return (mWidth == data.width() &&
                mHeight == data.height() &&
                mDepth == data.depth() &&
                mLayers == data.layers());
    };
    mDataWritten |= forWriting;
    if (!fileData.isNull() &&
            mTarget == fileData.target() &&
            (!mDataWritten || hasSameDimensions(fileData))) {
        mSystemCopyModified |= !mData.isSharedWith(fileData);
        mData = fileData;
    }

    // validate dimensions when writing
    if (mData.isNull() ||
            (forWriting && !hasSameDimensions(mData))) {
        if (!mData.create(mTarget, mFormat, mWidth, mHeight, mDepth, mLayers, mSamples)) {
            mData.create(mTarget, Texture::Format::RGBA8_UNorm, 1, 1, 1, 1, 1);
            mMessages += MessageList::insert(mItemId,
                MessageType::CreatingTextureFailed);
        }
        mData.clear();
        mSystemCopyModified |= true;
    }
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

    if (!mData.create(mTarget, mFormat, mWidth, mHeight, mDepth, mLayers, mSamples))
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
