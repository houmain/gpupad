#pragma once

#if defined(VULKAN_ENABLED)

#  include "render/Device.h"
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

    VKDevice();
    ~VKDevice() override;

    bool initialize(const AdapterIdentity &adapterIdentity) override;
    Lock lock();

private:
    SharedDevicePtr mShared;
};

void resetSharedVKDevice();

#endif // defined(VULKAN_ENABLED)
