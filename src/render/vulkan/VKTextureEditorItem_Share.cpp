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
    VkDevice getVkDevice(KDGpu::Device &device)
    {
        const auto &rm = *device.graphicsApi()->resourceManager();
        return static_cast<KDGpu::VulkanDevice *>(rm.getDevice(device))->device;
    }

    VkPhysicalDevice getVkPhysicalDevice(KDGpu::Device &device)
    {
        const auto &rm = *device.graphicsApi()->resourceManager();
        return rm.getAdapter(device.adapter()->handle())->physicalDevice;
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

VKTextureEditorItem::ShareState::~ShareState()
{
    Q_ASSERT(!vkTexture && !vkImage && !vkMemory);
}

bool VKTextureEditorItem::ShareState::initialize(VKContext &context,
    const ShareHandleData &shareHandle, const TextureData &data, int samples)
{
    const auto externalHandleType = toVkHandleType(shareHandle.type);
    if (!externalHandleType)
        return false;

    const auto format = toKDGpu(data.format());
    if (format == KDGpu::Format::UNDEFINED)
        return false;

    const auto vkDevice = getVkDevice(context.device);
    const auto vkPhysicalDevice = getVkPhysicalDevice(context.device);

    const auto kind = getKind(data.getTarget(samples), data.format());
    const auto levelCount =
        static_cast<uint32_t>(samples > 1 ? 1 : std::max(data.levels(), 1));
    const auto layerCount = vkArrayLayerCount(kind, data.layers());

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
        .extent = vkExtent(kind, data.width(), data.height(), data.depth()),
        .mipLevels = levelCount,
        .arrayLayers = layerCount,
        .samples =
            static_cast<VkSampleCountFlagBits>(getKDSampleCount(samples)),
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = imageUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    if (vkCreateImage(vkDevice, &imageInfo, nullptr, &vkImage))
        return false;

    auto requirements = VkMemoryRequirements{};
    vkGetImageMemoryRequirements(vkDevice, vkImage, &requirements);
    const auto memoryTypeIndex = findMemoryType(vkPhysicalDevice,
        requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryTypeIndex == VK_MAX_MEMORY_TYPES)
        return false;

    auto dedicatedInfo = VkMemoryDedicatedAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .image = vkImage,
    };
    const auto dedicated = shareHandle.dedicated
        || shareHandle.type == ShareHandleType::D3D12_RESOURCE
        || shareHandle.type == ShareHandleType::D3D11_IMAGE
        || shareHandle.type == ShareHandleType::D3D11_IMAGE_KMT;

#if defined(_WIN32)
    const auto importInfo = VkImportMemoryWin32HandleInfoKHR{
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
        .pNext = (dedicated ? &dedicatedInfo : nullptr),
        .handleType = *externalHandleType,
        .handle = static_cast<HANDLE>(shareHandle.handle),
    };
#else
    auto fd = static_cast<int>(reinterpret_cast<intptr_t>(shareHandle.handle));
    if (fd >= 0)
        fd = dup(fd);
    const auto importInfo = VkImportMemoryFdInfoKHR{
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
        .pNext = (dedicated ? &dedicatedInfo : nullptr),
        .handleType = *externalHandleType,
        .fd = fd,
    };
#endif
    const auto allocateInfo = VkMemoryAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &importInfo,
        .allocationSize = shareHandle.allocationSize
            ? static_cast<VkDeviceSize>(shareHandle.allocationSize)
            : requirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };
    if (vkAllocateMemory(vkDevice, &allocateInfo, nullptr, &vkMemory))
        return false;

    if (vkBindImageMemory(vkDevice, vkImage, vkMemory,
            static_cast<VkDeviceSize>(shareHandle.allocationOffset)))
        return false;

    const auto vkApi =
        static_cast<KDGpu::VulkanGraphicsApi *>(context.device.graphicsApi());
    const auto textureUsage =
        KDGpu::TextureUsageFlags{ KDGpu::TextureUsageFlagBits::SampledBit
            | KDGpu::TextureUsageFlagBits::TransferSrcBit
            | KDGpu::TextureUsageFlagBits::TransferDstBit };
    const auto textureOptions = KDGpu::TextureOptions{
        .type = getKDTextureType(kind),
        .format = format,
        .extent = {
            static_cast<uint32_t>(std::max(data.width(), 1)),
            static_cast<uint32_t>(std::max(data.height(), 1)),
            static_cast<uint32_t>(std::max(data.depth(), 1)),
        },
        .mipLevels = levelCount,
        .arrayLayers = layerCount,
        .samples = getKDSampleCount(samples),
        .usage = textureUsage,
        .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
        .initialLayout = KDGpu::TextureLayout::Undefined,
    };
    auto texture = vkApi->createTextureFromExistingVkImage(context.device,
        textureOptions, vkImage);
    if (!texture.isValid())
        return false;
    vkTexture = std::make_unique<VKTexture>(data, samples, std::move(texture));
    return true;
}

void VKTextureEditorItem::ShareState::release(KDGpu::Device &device)
{
    if (vkTexture)
        vkTexture->release(device);
    vkTexture.reset();

    if (vkImage)
        vkDestroyImage(getVkDevice(device), vkImage, nullptr);
    vkImage = {};

    if (vkMemory)
        vkFreeMemory(getVkDevice(device), vkMemory, nullptr);
    vkMemory = {};
}

bool VKTextureEditorItem::copySharedTexture(VKContext &context,
    const ShareHandleData &shareHandle, const TextureData &data)
{
    if (!mShare) {
        auto share = std::make_unique<VKTextureEditorItem::ShareState>();
        if (!share->initialize(context, shareHandle, data, mTextureSamples))
            return false;
        mShare = std::move(share);
    }

    if (!mShare)
        return false;

    if (!mTexture) {
        mTexture = std::make_unique<VKTexture>(data, mTextureSamples);
        mTexture->boundAsSampler();
    }
    if (!mTexture->copy(context, *mShare->vkTexture))
        return false;

    return true;
}
