#pragma once

#if defined(VULKAN_ENABLED)

// TODO: added because of multiple definitions of fmt::v11::detail::assert_fail
#  if defined(_WIN32) && !defined(FMT_ASSERT)
#    define FMT_ASSERT
#  endif

#  include "render/Device.h"
#  include "render/AdapterIdentity.h"
#  include <memory>
#  include <mutex>

namespace KDGpu {
    class Adapter;
    class Device;
    class Queue;
    class Instance;
    class Surface;
} // namespace KDGpu

struct ktxVulkanDeviceInfo;

class VKDevice final : public Device
{
public:
    struct SharedDevice;
    using SharedDevicePtr = std::shared_ptr<SharedDevice>;

    class Lock
    {
    public:
        Lock() = default;
        explicit Lock(SharedDevicePtr shared);

        KDGpu::Instance &instance();
        KDGpu::Adapter &adapter();
        KDGpu::Device &device();
        KDGpu::Queue &queue();
        ktxVulkanDeviceInfo &ktxDeviceInfo();

    private:
        SharedDevicePtr mShared;
        std::unique_lock<std::recursive_mutex> mLock;
    };

    static KDGpu::Instance &instance();
    static void resetSharedDevice();

    VKDevice(const AdapterIdentity &adapterIdentity, const QString &apiVersion);
    ~VKDevice() override;
    bool initialize() override;
    Lock lock();

private:
    SharedDevicePtr mShared;
};

#endif // defined(VULKAN_ENABLED)
