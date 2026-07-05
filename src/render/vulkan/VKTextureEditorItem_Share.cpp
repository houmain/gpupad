#include "VKTextureEditorItem.h"
#include "KDGpuEnums.h"
#include "VKTexture.h"
#include "VKWindow.h"

#if defined(_WIN32)
#  include <vulkan/vulkan_win32.h>
#else
#  include <unistd.h>
#endif

namespace {
    KDGpu::VulkanDevice *getVkDevice(KDGpu::Device &device)
    {
        const auto &rm = *device.graphicsApi()->resourceManager();
        return static_cast<KDGpu::VulkanDevice *>(rm.getDevice(device));
    }

    std::optional<VkExternalMemoryHandleTypeFlagBits> toVkHandleType(
        ShareHandleType type)
    {
        switch (type) {
        case ShareHandleType::OPAQUE_FD:
            return VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
        case ShareHandleType::OPAQUE_WIN32:
            return VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
        case ShareHandleType::OPAQUE_WIN32_KMT:
            return VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
        case ShareHandleType::D3D11_IMAGE:
            return VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
        case ShareHandleType::D3D11_IMAGE_KMT:
            return VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;
        case ShareHandleType::D3D12_TILEPOOL:
            return VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT;
        case ShareHandleType::D3D12_RESOURCE:
            return VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
        default: return {};
        }
    }

    VkImageType toVkImageType(const TextureKind &kind)
    {
        if (kind.cubeMap)
            return VK_IMAGE_TYPE_2D;

        switch (kind.dimensions) {
        case 1: return VK_IMAGE_TYPE_1D;
        case 2: return VK_IMAGE_TYPE_2D;
        case 3: return VK_IMAGE_TYPE_3D;
        }
        return VK_IMAGE_TYPE_2D;
    }

    VkExtent3D vkExtent(const TextureKind &kind, int width, int height,
        int depth)
    {
        return {
            .width = static_cast<uint32_t>(std::max(width, 1)),
            .height = static_cast<uint32_t>(
                kind.dimensions >= 2 ? std::max(height, 1) : 1),
            .depth = static_cast<uint32_t>(
                kind.dimensions == 3 ? std::max(depth, 1) : 1),
        };
    }

    uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
        uint32_t memoryTypeBits, VkMemoryPropertyFlags preferredFlags)
    {
        auto memoryProperties = VkPhysicalDeviceMemoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

        for (auto i = 0u; i < memoryProperties.memoryTypeCount; ++i)
            if ((memoryTypeBits & (1u << i))
                && (memoryProperties.memoryTypes[i].propertyFlags
                       & preferredFlags)
                    == preferredFlags)
                return i;

        for (auto i = 0u; i < memoryProperties.memoryTypeCount; ++i)
            if (memoryTypeBits & (1u << i))
                return i;

        return VK_MAX_MEMORY_TYPES;
    }

} // namespace

void VKTextureEditorItem::releaseShareState()
{
    if (!mShare)
        return;

    if (window().initialized()) {
        auto &device = window().device();
        if (mShare->texture) {
            mShare->texture->release(device);
            mShare->texture.reset();
        }

        if (auto vkDevice = getVkDevice(device)) {
            if (auto image = std::exchange(mShare->image, {}))
                vkDestroyImage(vkDevice->device, image, nullptr);
            if (auto memory = std::exchange(mShare->memory, {}))
                vkFreeMemory(vkDevice->device, memory, nullptr);
        }
    } else {
        Q_ASSERT(!mShare->texture && !mShare->image && !mShare->memory);
    }

    mShare.reset();
}

bool VKTextureEditorItem::copyImportedTexture(ShareHandle textureHandle)
{
    auto context = makeContext();

    context.commandRecorder =
        context.device.createCommandRecorder({ .queue = context.queue });
    if (!importShareHandle(context, std::move(textureHandle))) {
        if (mTexture)
            mTexture->release(window().device());
        mTexture.reset();
        releaseShareState();
        return false;
    }
    if (!copyShareStateToTexture(context)) {
        if (mTexture)
            mTexture->release(window().device());
        mTexture.reset();
        return false;
    }

    submitCommandQueue(context);
    return (mTexture && mTexture->texture().isValid());
}

bool VKTextureEditorItem::importShareHandle(VKContext &context,
    ShareHandle shareHandle)
{
    if (!shareHandle)
        return false;

    auto externalHandleType = toVkHandleType(shareHandle.type);
    if (!externalHandleType) {
        qWarning() << "Unsupported shared texture handle type";
        return false;
    }

    if (mShare && mShare->shareHandle.sameResource(shareHandle)
        && mShare->texture && mShare->texture->texture().isValid()
        && mShare->texture->samples() == mTextureSamples) {
        mShare->shareHandle = std::move(shareHandle);
        return true;
    }

    const auto format = toKDGpu(mImage.format());
    if (format == KDGpu::Format::UNDEFINED) {
        qWarning() << "Unsupported Vulkan shared texture import format";
        return false;
    }

    releaseShareState();

    const auto &rm = *context.device.graphicsApi()->resourceManager();
    const auto vkDevice = getVkDevice(context.device);
    const auto vkAdapter = rm.getAdapter(context.device.adapter()->handle());

    const auto kind =
        getKind(mImage.getTarget(mTextureSamples), mImage.format());
    const auto levelCount = static_cast<uint32_t>(
        mTextureSamples > 1 ? 1 : std::max(mImage.levels(), 1));
    const auto layerCount = vkArrayLayerCount(kind, mImage.layers());

    mShare.reset(new ShareState);
    auto &imported = *mShare;

    const auto imageUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    const auto imageCreateFlags =
        kind.cubeMap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    auto externalImageInfo = VkExternalMemoryImageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes =
            static_cast<VkExternalMemoryHandleTypeFlags>(*externalHandleType),
    };

    const auto imageInfo = VkImageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &externalImageInfo,
        .flags = static_cast<VkImageCreateFlags>(imageCreateFlags),
        .imageType = toVkImageType(kind),
        .format = static_cast<VkFormat>(format),
        .extent =
            vkExtent(kind, mImage.width(), mImage.height(), mImage.depth()),
        .mipLevels = levelCount,
        .arrayLayers = layerCount,
        .samples = static_cast<VkSampleCountFlagBits>(
            getKDSampleCount(mTextureSamples)),
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = imageUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if (vkCreateImage(vkDevice->device, &imageInfo, nullptr, &imported.image)
        != VK_SUCCESS) {
        qWarning() << "Creating imported Vulkan shared texture image failed";
        releaseShareState();
        return false;
    }

    auto requirements = VkMemoryRequirements{};
    vkGetImageMemoryRequirements(vkDevice->device, imported.image,
        &requirements);
    const auto memoryTypeIndex = findMemoryType(vkAdapter->physicalDevice,
        requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryTypeIndex == VK_MAX_MEMORY_TYPES) {
        qWarning() << "No compatible Vulkan memory type for shared texture import";
        releaseShareState();
        return false;
    }

    auto dedicatedInfo = VkMemoryDedicatedAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .image = imported.image,
    };
    auto allocateInfo = VkMemoryAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = shareHandle.allocationSize
            ? static_cast<VkDeviceSize>(shareHandle.allocationSize)
            : requirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };

    const auto dedicated = shareHandle.dedicated
        || shareHandle.type == ShareHandleType::D3D12_RESOURCE
        || shareHandle.type == ShareHandleType::D3D11_IMAGE
        || shareHandle.type == ShareHandleType::D3D11_IMAGE_KMT;

#if defined(_WIN32)
    auto importInfo = VkImportMemoryWin32HandleInfoKHR{
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
        .handleType = *externalHandleType,
        .handle = static_cast<HANDLE>(shareHandle.handle),
    };
    if (dedicated) {
        importInfo.pNext = &dedicatedInfo;
        allocateInfo.pNext = &importInfo;
    } else {
        allocateInfo.pNext = &importInfo;
    }
#else
    auto fd = static_cast<int>(reinterpret_cast<intptr_t>(shareHandle.handle));
    if (fd >= 0)
        fd = dup(fd);
    auto importInfo = VkImportMemoryFdInfoKHR{
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
        .handleType = *externalHandleType,
        .fd = fd,
    };
    if (dedicated) {
        importInfo.pNext = &dedicatedInfo;
        allocateInfo.pNext = &importInfo;
    } else {
        allocateInfo.pNext = &importInfo;
    }
#endif

    if (vkAllocateMemory(vkDevice->device, &allocateInfo, nullptr,
            &imported.memory)
        != VK_SUCCESS) {
        qWarning() << "Importing Vulkan shared texture memory failed";
        releaseShareState();
        return false;
    }
    if (!imported.memory) {
        qWarning() << "Importing Vulkan shared texture memory returned a null handle";
        releaseShareState();
        return false;
    }

    if (vkBindImageMemory(vkDevice->device, imported.image, imported.memory,
            static_cast<VkDeviceSize>(shareHandle.allocationOffset))
        != VK_SUCCESS) {
        qWarning() << "Binding Vulkan shared texture memory failed";
        releaseShareState();
        return false;
    }

    auto vkApi =
        static_cast<KDGpu::VulkanGraphicsApi *>(context.device.graphicsApi());
    const auto usage =
        KDGpu::TextureUsageFlags{ KDGpu::TextureUsageFlagBits::SampledBit
            | KDGpu::TextureUsageFlagBits::TransferSrcBit
            | KDGpu::TextureUsageFlagBits::TransferDstBit };
    auto texture = vkApi->createTextureFromExistingVkImage(context.device,
        KDGpu::TextureOptions{
            .type = getKDTextureType(kind),
            .format = format,
            .extent = {
                static_cast<uint32_t>(std::max(mImage.width(), 1)),
                static_cast<uint32_t>(std::max(mImage.height(), 1)),
                static_cast<uint32_t>(std::max(mImage.depth(), 1)),
            },
            .mipLevels = levelCount,
            .arrayLayers = layerCount,
            .samples = getKDSampleCount(mTextureSamples),
            .usage = usage,
            .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
            .initialLayout = KDGpu::TextureLayout::Undefined,
        },
        imported.image);
    if (!texture.isValid()) {
        releaseShareState();
        return false;
    }

    imported.texture = std::make_unique<VKTexture>(mImage, mTextureSamples,
        std::move(texture));
    if (!imported.texture->prepareSampledImage(context)) {
        releaseShareState();
        return false;
    }

    imported.shareHandle = std::move(shareHandle);
    return true;
}

bool VKTextureEditorItem::copyShareStateToTexture(VKContext &context)
{
    if (!mShare || !mShare->texture || !mShare->texture->texture().isValid())
        return false;

    if (!mTexture || mTexture->samples() != mTextureSamples) {
        if (mTexture)
            mTexture->release(context.device);
        mTexture = std::make_unique<VKTexture>(mImage, mTextureSamples);
        mTexture->boundAsSampler();
    }

    if (!mTexture->copy(context, *mShare->texture))
        return false;
    if (!mTexture->prepareSampledImage(context))
        return false;

    resetTextureBinding();
    return true;
}
