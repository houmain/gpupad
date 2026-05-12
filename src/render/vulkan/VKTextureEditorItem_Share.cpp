#include "VKTextureEditorItem.h"
#include "KDGpuEnums.h"
#include "VKTexture.h"
#include "VKWindow.h"
#include <KDGpu/command_recorder.h>
#include <KDGpu/device.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/vulkan/vulkan_adapter.h>
#include <KDGpu/vulkan/vulkan_device.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>

#if defined(_WIN32)
#  if !defined(NOMINMAX)
#    define NOMINMAX
#  endif
#  if !defined(WIN32_LEAN_AND_MEAN)
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <vulkan/vulkan_win32.h>
#else
#  include <unistd.h>
#endif

struct VKTextureEditorItem::ShareState
{
    VkDevice device{};
    VkImage image{};
    VkDeviceMemory memory{};
    ShareHandle shareHandle{};
};

void VKTextureEditorItem::ShareStateDeleter::operator()(ShareState *state) const
{
    if (!state)
        return;
    if (state->device) {
        if (auto image = std::exchange(state->image, {}))
            vkDestroyImage(state->device, image, nullptr);
        if (auto memory = std::exchange(state->memory, {}))
            vkFreeMemory(state->device, memory, nullptr);
    }
    delete state;
}

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

    uint32_t vkArrayLayerCount(const TextureKind &kind, int layers)
    {
        return static_cast<uint32_t>(
            std::max(layers, 1) * (kind.cubeMap ? 6 : 1));
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

bool VKTextureEditorItem::updateImportedTexture()
{
    if (!mUpload && mShare && mShare->shareHandle == mPreviewTextureHandle
        && mTexture && mTexture->texture().isValid()
        && mTexture->samples() == mTextureSamples)
        return true;

    auto context = makeContext();
    context.commandRecorder =
        context.device.createCommandRecorder({ .queue = context.queue });
    if (!importShareHandle(context, mPreviewTextureHandle)) {
        if (mTexture)
            mTexture->release(window().device());
        mTexture.reset();
        mShare.reset();
        return false;
    }
    submitCommandQueue(context);
    mUpload = false;
    return (mTexture && mTexture->texture().isValid());
}

bool VKTextureEditorItem::importShareHandle(VKContext &context,
    ShareHandle shareHandle)
{
    if (!shareHandle)
        return false;

    auto externalHandleType = toVkHandleType(shareHandle.type);
    if (!externalHandleType) {
        qWarning() << "Unsupported shared preview texture handle type";
        return false;
    }

    if (!mUpload && mShare && mShare->shareHandle == shareHandle && mTexture
        && mTexture->texture().isValid()
        && mTexture->samples() == mTextureSamples)
        return true;

    const auto format = toKDGpu(mImage.format());
    if (format == KDGpu::Format::UNDEFINED) {
        qWarning() << "Unsupported Vulkan preview import format";
        return false;
    }

    if (mTexture)
        mTexture->release(context.device);
    mTexture.reset();
    mShare.reset();
    resetTextureBinding();

    const auto &rm = *context.device.graphicsApi()->resourceManager();
    const auto vkDevice =
        static_cast<KDGpu::VulkanDevice *>(rm.getDevice(context.device));
    const auto vkAdapter = rm.getAdapter(context.device.adapter()->handle());

    const auto kind =
        getKind(mImage.getTarget(mTextureSamples), mImage.format());
    const auto levelCount = static_cast<uint32_t>(
        mTextureSamples > 1 ? 1 : std::max(mImage.levels(), 1));
    const auto layerCount = vkArrayLayerCount(kind, mImage.layers());

    auto imported =
        std::unique_ptr<ShareState, ShareStateDeleter>(new ShareState);
    imported->device = vkDevice->device;

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

    if (vkCreateImage(vkDevice->device, &imageInfo, nullptr, &imported->image)
        != VK_SUCCESS) {
        qWarning() << "Creating imported Vulkan preview image failed";
        return false;
    }

    auto requirements = VkMemoryRequirements{};
    vkGetImageMemoryRequirements(vkDevice->device, imported->image,
        &requirements);
    const auto memoryTypeIndex = findMemoryType(vkAdapter->physicalDevice,
        requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryTypeIndex == VK_MAX_MEMORY_TYPES) {
        qWarning() << "No compatible Vulkan memory type for preview import";
        return false;
    }

    auto dedicatedInfo = VkMemoryDedicatedAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .image = imported->image,
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
            &imported->memory)
        != VK_SUCCESS) {
        qWarning() << "Importing Vulkan preview memory failed";
        return false;
    }

    if (vkBindImageMemory(vkDevice->device, imported->image, imported->memory,
            static_cast<VkDeviceSize>(shareHandle.allocationOffset))
        != VK_SUCCESS) {
        qWarning() << "Binding Vulkan preview memory failed";
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
        imported->image);
    if (!texture.isValid())
        return false;

    auto gpuTexture = std::make_unique<VKTexture>(mImage, mTextureSamples,
        std::move(texture));
    if (!gpuTexture->prepareSampledImage(context)) {
        gpuTexture->release(context.device);
        return false;
    }

    imported->shareHandle = shareHandle;
    mTexture = std::move(gpuTexture);
    mShare = std::move(imported);
    resetTextureBinding();
    return true;
}
