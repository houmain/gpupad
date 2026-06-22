#include "VKDevice.h"

#if defined(VULKAN_ENABLED)

#  include <QCoreApplication>
#  include <QDebug>
#  include <KDGpu/adapter.h>
#  include <KDGpu/surface.h>
#  include <KDGpu/vulkan/vulkan_adapter.h>
#  include <KDGpu/vulkan/vulkan_device.h>
#  include <KDGpu/vulkan/vulkan_graphics_api.h>
#  include <KDGpu/vulkan/vulkan_instance.h>
#  include <KDGpu/vulkan/vulkan_queue.h>
#  include <algorithm>
#  include <cstdio>
#  include <optional>
#  include <vector>

#  if defined(_WIN32)
#    define NOMINMAX
#    define WIN32_LEAN_AND_MEAN
#    include <spdlog/sinks/msvc_sink.h>
#    include <vulkan/vulkan_win32.h>
#  endif

namespace {
    uint32_t applicationVersion()
    {
        auto major = 0;
        auto minor = 0;
        auto build = 0;
        std::sscanf(qPrintable(QCoreApplication::applicationVersion()),
            "%d.%d.%d", &major, &minor, &build);
        return KDGPU_MAKE_API_VERSION(0, major, minor, build);
    }

    template <typename Range>
    bool isNull(const Range &range)
    {
        return std::ranges::all_of(range,
            [](const auto byte) { return byte == 0; });
    }

    bool hasUUID(const AdapterIdentity &adapterIdentity)
    {
        if (isNull(adapterIdentity.driverUUID))
            return false;

        return std::ranges::any_of(adapterIdentity.deviceUUIDs,
            [](const auto &deviceUUID) { return !isNull(deviceUUID); });
    }

    bool hasStableIdentity(const AdapterIdentity &adapterIdentity)
    {
        return !isNull(adapterIdentity.deviceLUID) || hasUUID(adapterIdentity);
    }

    bool matchesAdapter(const KDGpu::Adapter &adapter,
        const AdapterIdentity &adapterIdentity)
    {
        const auto &properties = adapter.properties();
        if (!isNull(adapterIdentity.deviceLUID)
            && std::ranges::equal(properties.deviceLUID,
                adapterIdentity.deviceLUID))
            return true;

        if (!isNull(adapterIdentity.driverUUID)
            && std::ranges::equal(properties.driverUUID,
                adapterIdentity.driverUUID)) {
            for (const auto &deviceUUID : adapterIdentity.deviceUUIDs)
                if (!isNull(deviceUUID)
                    && std::ranges::equal(properties.deviceUUID, deviceUUID))
                    return true;
        }

        return false;
    }

    KDGpu::Adapter *findAdapter(KDGpu::Instance &instance,
        const AdapterIdentity &adapterIdentity)
    {
        if (hasStableIdentity(adapterIdentity)) {
            for (auto adapter : instance.adapters())
                if (matchesAdapter(*adapter, adapterIdentity))
                    return adapter;
            return nullptr;
        }

        if (auto adapter =
                instance.selectAdapter(KDGpu::AdapterDeviceType::Default))
            return adapter;

        if (!instance.adapters().empty())
            return instance.adapters().front();

        return nullptr;
    }

    std::optional<uint32_t> findQueueType(KDGpu::Adapter &adapter,
        KDGpu::Surface *surface, bool computeRequired)
    {
        const auto requiredQueueFlags =
            KDGpu::QueueFlags(KDGpu::QueueFlagBits::GraphicsBit
                | KDGpu::QueueFlagBits::TransferBit
                | (computeRequired ? KDGpu::QueueFlagBits::ComputeBit
                                   : KDGpu::QueueFlags{}));

        const auto queueTypes = adapter.queueTypes();
        for (auto i = 0u; i < queueTypes.size(); ++i) {
            if (!queueTypes[i].supportsFeature(requiredQueueFlags))
                continue;
            if (surface && !adapter.supportsPresentation(*surface, i))
                continue;
            return i;
        }

        if (!surface)
            return {};

        for (auto i = 0u; i < queueTypes.size(); ++i) {
            if (!queueTypes[i].supportsFeature(
                    KDGpu::QueueFlags(KDGpu::QueueFlagBits::GraphicsBit)))
                continue;
            if (!adapter.supportsPresentation(*surface, i))
                continue;
            return i;
        }
        return {};
    }

    std::weak_ptr<VKDevice> sSharedVKDevice;
} // namespace

VKDevice::VKDevice() : Device(Type::Vulkan) { }

VKDevice::~VKDevice()
{
    if (mDevice.isValid())
        mDevice.waitUntilIdle();

    releaseKtx();
    mQueue = {};
    mDevice = {};
    mAdapter = {};
    mInstance = {};
    mApi.reset();
}

bool VKDevice::initialize(const AdapterIdentity &adapterIdentity)
{
    if (mDevice.isValid())
        return true;

    if (!ensureInstance())
        return false;

    mAdapter = [&]() -> KDGpu::Adapter * {
        for (auto adapter : mInstance.adapters())
            if (std::ranges::equal(adapter->properties().driverUUID,
                    adapterIdentity.driverUUID))
                for (const auto &deviceUUID : adapterIdentity.deviceUUIDs)
                    if (std::ranges::equal(adapter->properties().deviceUUID,
                            deviceUUID))
                        return adapter;
        return nullptr;
    }();

    if (!mAdapter) {
        if (mInstance.adapters().empty()) {
            error(MessageType::VulkanNotAvailable);
            return false;
        }
        mAdapter = mInstance.adapters().front();
    }

    KDGpu::Logger::logger()->info("Initializing Vulkan on the "
        + adapterDeviceTypeToString(mAdapter->properties().deviceType) + " "
        + mAdapter->properties().deviceName);

    mDevice = mAdapter->createDevice(KDGpu::DeviceOptions{
        .requestedFeatures = mAdapter->features(),
    });
    if (!mDevice.isValid()) {
        error(MessageType::VulkanNotAvailable, "creating device failed");
        return false;
    }

    if (!chooseQueue(nullptr, true)) {
        error(MessageType::VulkanNotAvailable, "no general queue found");
        return false;
    }

    if (!initializeKtx())
        return false;
    return true;
}

bool VKDevice::initialize(KDGpu::Surface &surface,
    const AdapterIdentity &adapterIdentity)
{
    if (!ensureInstance())
        return false;

    if (!mDevice.isValid()) {
        mAdapter = findAdapter(mInstance, adapterIdentity);
        if (!mAdapter) {
            qWarning() << "Finding selected KDGpu Vulkan adapter failed";
            return false;
        }

        const auto queueTypeIndex = findQueueType(*mAdapter, &surface, false);
        if (!queueTypeIndex) {
            qWarning() << "No KDGpu Vulkan graphics/present queue found";
            return false;
        }

        mDevice = mAdapter->createDevice(KDGpu::DeviceOptions{
            .queues = { KDGpu::QueueRequest{
                .queueTypeIndex = *queueTypeIndex,
                .count = 1,
                .priorities = { 1.0f },
            } },
            .requestedFeatures = mAdapter->features(),
        });
        if (!mDevice.isValid()) {
            qWarning() << "Creating KDGpu Vulkan device failed";
            return false;
        }

        if (!chooseQueue(&surface, false)) {
            qWarning() << "No KDGpu Vulkan graphics/present queue found";
            return false;
        }
    } else if (hasStableIdentity(adapterIdentity)
        && !matchesAdapter(*mAdapter, adapterIdentity)) {
        qWarning() << "Shared KDGpu device uses a different adapter";
        return false;
    } else if (
        !mAdapter->supportsPresentation(surface, mQueue.queueTypeIndex())) {
        qWarning() << "Shared KDGpu device queue cannot present to this window";
        return false;
    }

    if (!initializeKtx())
        return false;

    return true;
}

bool VKDevice::hasAdapters()
{
    return ensureInstance() && !mInstance.adapters().empty();
}

KDGpu::Instance &VKDevice::instance()
{
    Q_ASSERT(mInstance.isValid());
    return mInstance;
}

KDGpu::Adapter &VKDevice::adapter()
{
    Q_ASSERT(mAdapter);
    return *mAdapter;
}

KDGpu::Device &VKDevice::device()
{
    Q_ASSERT(mDevice.isValid());
    return mDevice;
}

KDGpu::Queue &VKDevice::queue()
{
    Q_ASSERT(mQueue.isValid());
    return mQueue;
}

ktxVulkanDeviceInfo &VKDevice::ktxDeviceInfo()
{
    Q_ASSERT(mKtxDeviceInfoInitialized);
    return mKtxDeviceInfo;
}

bool VKDevice::ensureInstance()
{
    if (mInstance.isValid())
        return true;

#  if defined(_WIN32)
    static auto logger =
        spdlog::synchronous_factory::create<spdlog::sinks::msvc_sink_mt>(
            "KDGpu");
#  endif

    mApi = std::make_unique<KDGpu::VulkanGraphicsApi>();
    mInstance = mApi->createInstance(KDGpu::InstanceOptions{
        .applicationName = QCoreApplication::applicationName().toStdString(),
        .applicationVersion = applicationVersion(),
#  if !defined(NDEBUG)
        .layers = { "VK_LAYER_KHRONOS_validation" },
#  endif
    });

    if (!mInstance.isValid() || mInstance.adapters().empty()) {
        error(MessageType::VulkanNotAvailable);
        return false;
    }
    return true;
}

bool VKDevice::chooseQueue(KDGpu::Surface *surface, bool computeRequired)
{
    const auto requiredQueueFlags = KDGpu::QueueFlags(
        KDGpu::QueueFlagBits::GraphicsBit | KDGpu::QueueFlagBits::TransferBit
        | (computeRequired ? KDGpu::QueueFlagBits::ComputeBit
                           : KDGpu::QueueFlags{}));

    for (const auto &candidate : mDevice.queues()) {
        if ((candidate.flags() & requiredQueueFlags) != requiredQueueFlags)
            continue;
        if (surface
            && !mAdapter->supportsPresentation(*surface,
                candidate.queueTypeIndex()))
            continue;
        mQueue = candidate;
        return true;
    }

    if (!surface)
        return false;

    for (const auto &candidate : mDevice.queues()) {
        if (!(candidate.flags() & KDGpu::QueueFlagBits::GraphicsBit))
            continue;
        if (!mAdapter->supportsPresentation(*surface,
                candidate.queueTypeIndex()))
            continue;
        mQueue = candidate;
        return true;
    }
    return false;
}

bool VKDevice::initializeKtx()
{
    if (mKtxDeviceInfoInitialized)
        return true;

    const auto &rm = *mDevice.graphicsApi()->resourceManager();
    const auto vkInstance = rm.getInstance(mInstance);
    const auto vkAdapter = rm.getAdapter(*mAdapter);
    const auto vkDevice = rm.getDevice(mDevice);
    const auto vkQueue = rm.getQueue(mQueue);

    const auto commandPoolInfo = VkCommandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = mQueue.queueTypeIndex(),
    };
    if (vkCreateCommandPool(vkDevice->device, &commandPoolInfo, nullptr,
            &mKtxCommandPool)
        != VK_SUCCESS) {
        error(MessageType::VulkanNotAvailable, "creating command pool failed");
        return false;
    }

    if (ktxVulkanDeviceInfo_ConstructEx(&mKtxDeviceInfo, vkInstance->instance,
            vkAdapter->physicalDevice, vkDevice->device, vkQueue->queue,
            mKtxCommandPool, nullptr, nullptr)
        != KTX_SUCCESS) {
        error(MessageType::VulkanNotAvailable,
            "creating device info for KTX failed");
        releaseKtx();
        return false;
    }

    mKtxDeviceInfoInitialized = true;
    return true;
}

void VKDevice::releaseKtx()
{
    if (mKtxDeviceInfoInitialized) {
        ktxVulkanDeviceInfo_Destruct(&mKtxDeviceInfo);
        mKtxDeviceInfoInitialized = false;
        mKtxDeviceInfo = {};
    }

    if (mKtxCommandPool && mDevice.isValid()) {
        const auto &rm = *mDevice.graphicsApi()->resourceManager();
        const auto vkDevice =
            static_cast<KDGpu::VulkanDevice *>(rm.getDevice(mDevice));
        vkDestroyCommandPool(vkDevice->device, mKtxCommandPool, nullptr);
        mKtxCommandPool = VK_NULL_HANDLE;
    }
}

void VKDevice::error(MessageType messageType, const QString &message)
{
    mMessages.insert(0, messageType, message);
}

std::shared_ptr<VKDevice> sharedVKDevice()
{
    auto device = sSharedVKDevice.lock();
    if (!device) {
        device = std::make_shared<VKDevice>();
        sSharedVKDevice = device;
    }
    return device;
}

void resetSharedVKDevice()
{
    sSharedVKDevice.reset();
}

#endif // defined(VULKAN_ENABLED)
