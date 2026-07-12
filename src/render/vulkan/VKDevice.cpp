#if defined(VULKAN_ENABLED)
#  include "VKDevice.h"
#  include "MessageList.h"
#  include <QCoreApplication>
#  include <KDGpu/adapter.h>
#  include <KDGpu/vulkan/vulkan_adapter.h>
#  include <KDGpu/vulkan/vulkan_device.h>
#  include <KDGpu/vulkan/vulkan_graphics_api.h>
#  include <KDGpu/vulkan/vulkan_instance.h>
#  include <KDGpu/vulkan/vulkan_queue.h>
#  include <ktxvulkan.h>
#  include <algorithm>
#  include <cstdio>
#  include <mutex>
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

    bool matchesAdapter(const KDGpu::Adapter &adapter,
        const AdapterIdentity &adapterIdentity)
    {
        const auto matches = [](const auto &a, const auto &b) {
            if (std::ranges::all_of(a,
                    [](const auto byte) { return byte == 0; }))
                return false;
            return std::ranges::equal(a, b);
        };
        const auto &properties = adapter.properties();
        if (matches(properties.deviceLUID, adapterIdentity.deviceLUID))
            return true;
        if (matches(properties.driverUUID, adapterIdentity.driverUUID))
            for (const auto &deviceUUID : adapterIdentity.deviceUUIDs)
                if (matches(properties.deviceUUID, deviceUUID))
                    return true;
        return false;
    }

    std::mutex gSharedVKDeviceMutex;
    VKDevice::SharedDevicePtr gSharedVKDevice;
} // namespace

struct VKDevice::SharedDevice
{
    SharedDevice();
    ~SharedDevice();
    bool initialize(const AdapterIdentity &adapterIdentity);
    bool initializeKtxDeviceInfo();
    void releaseKtxDeviceInfo();

    std::recursive_mutex mutex;
    std::unique_ptr<KDGpu::VulkanGraphicsApi> api;
    KDGpu::Instance instance;
    KDGpu::Adapter *adapter{};
    KDGpu::Device device;
    KDGpu::Queue queue;
    ktxVulkanDeviceInfo ktxDeviceInfo{};
    VkCommandPool ktxCommandPool{};
    MessagePtrSet messages;
};

VKDevice::SharedDevice::SharedDevice()
{
#  if defined(_WIN32)
    static auto logger =
        spdlog::synchronous_factory::create<spdlog::sinks::msvc_sink_mt>(
            "KDGpu");
#  endif

    api = std::make_unique<KDGpu::VulkanGraphicsApi>();
    instance = api->createInstance(KDGpu::InstanceOptions{
        .applicationName = QCoreApplication::applicationName().toStdString(),
        .applicationVersion = applicationVersion(),
#  if !defined(NDEBUG)
        .layers = { "VK_LAYER_KHRONOS_validation" },
#  endif
    });

    if (instance.isValid() && instance.adapters().empty())
        instance = {};
}

VKDevice::SharedDevice::~SharedDevice()
{
    const auto lockDevice = std::lock_guard{ mutex };
    if (device.isValid())
        device.waitUntilIdle();

    releaseKtxDeviceInfo();
}

bool VKDevice::SharedDevice::initialize(const AdapterIdentity &adapterIdentity)
{
    if (adapter)
        return (ktxDeviceInfo.device != nullptr);

    if (!instance.isValid()) {
        messages.insert(MessageType::VulkanNotAvailable);
        return false;
    }

    adapter = instance.selectAdapter(KDGpu::AdapterDeviceType::Default);
    for (auto canditate : instance.adapters())
        if (matchesAdapter(*canditate, adapterIdentity))
            adapter = canditate;

    if (!adapter) {
        messages.insert(MessageType::VulkanNotAvailable, "no adapter found");
        return false;
    }

    device = adapter->createDevice(KDGpu::DeviceOptions{
        .requestedFeatures = adapter->features(),
    });
    if (!device.isValid()) {
        messages.insert(MessageType::VulkanNotAvailable,
            "creating device failed");
        return false;
    }

    const auto requiredQueueFlags = KDGpu::QueueFlags(
        KDGpu::QueueFlagBits::GraphicsBit | KDGpu::QueueFlagBits::TransferBit
        | KDGpu::QueueFlagBits::ComputeBit);
    for (const auto &canditate : device.queues())
        if ((canditate.flags() & requiredQueueFlags) == requiredQueueFlags) {
            queue = canditate;
            break;
        }
    if (!queue.isValid()) {
        messages.insert(MessageType::VulkanNotAvailable, "no queue found");
        return false;
    }
    if (!initializeKtxDeviceInfo()) {
        messages.insert(MessageType::VulkanNotAvailable,
            "initializing KTX device info failed");
        return false;
    }
    return true;
}

bool VKDevice::SharedDevice::initializeKtxDeviceInfo()
{
    const auto &rm = *device.graphicsApi()->resourceManager();
    const auto vkInstance = rm.getInstance(instance);
    const auto vkAdapter = rm.getAdapter(*adapter);
    const auto vkDevice = rm.getDevice(device);
    const auto vkQueue = rm.getQueue(queue);

    const auto commandPoolInfo = VkCommandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue.queueTypeIndex(),
    };
    return (vkCreateCommandPool(vkDevice->device, &commandPoolInfo, nullptr,
                &ktxCommandPool)
            == VK_SUCCESS
        && ktxVulkanDeviceInfo_ConstructEx(&ktxDeviceInfo, vkInstance->instance,
               vkAdapter->physicalDevice, vkDevice->device, vkQueue->queue,
               ktxCommandPool, nullptr, nullptr)
            == KTX_SUCCESS);
}

void VKDevice::SharedDevice::releaseKtxDeviceInfo()
{
    if (ktxDeviceInfo.device)
        ktxVulkanDeviceInfo_Destruct(&ktxDeviceInfo);

    if (ktxCommandPool) {
        const auto &rm = *device.graphicsApi()->resourceManager();
        const auto vkDevice =
            static_cast<KDGpu::VulkanDevice *>(rm.getDevice(device));
        vkDestroyCommandPool(vkDevice->device, ktxCommandPool, nullptr);
        ktxCommandPool = VK_NULL_HANDLE;
    }
}

VKDevice::Lock::Lock(SharedDevicePtr shared)
    : mShared(std::move(shared))
    , mLock(mShared->mutex)
{
}

KDGpu::Instance &VKDevice::Lock::instance()
{
    Q_ASSERT(mShared->instance.isValid());
    return mShared->instance;
}

KDGpu::Adapter &VKDevice::Lock::adapter()
{
    Q_ASSERT(mShared->adapter);
    return *mShared->adapter;
}

KDGpu::Device &VKDevice::Lock::device()
{
    Q_ASSERT(mShared->device.isValid());
    return mShared->device;
}

KDGpu::Queue &VKDevice::Lock::queue()
{
    Q_ASSERT(mShared->queue.isValid());
    return mShared->queue;
}

ktxVulkanDeviceInfo &VKDevice::Lock::ktxDeviceInfo()
{
    Q_ASSERT(mShared->ktxDeviceInfo.device);
    return mShared->ktxDeviceInfo;
}

//-------------------------------------------------------------------------

void resetSharedVKDevice()
{
    const auto lock = std::lock_guard{ gSharedVKDeviceMutex };
    gSharedVKDevice.reset();
}

VKDevice::VKDevice() : Device(Type::Vulkan)
{
    const auto lock = std::lock_guard{ gSharedVKDeviceMutex };
    if (!gSharedVKDevice)
        gSharedVKDevice = std::make_shared<VKDevice::SharedDevice>();
    mShared = gSharedVKDevice;
}

VKDevice::~VKDevice() = default;

bool VKDevice::initialize(const AdapterIdentity &adapterIdentity)
{
    return mShared->initialize(adapterIdentity);
}

VKDevice::Lock VKDevice::lock()
{
    return Lock{ mShared };
}

#endif // defined(VULKAN_ENABLED)
