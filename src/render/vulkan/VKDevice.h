#pragma once

#if defined(VULKAN_ENABLED)

#  include "render/Device.h"
#  include "MessageList.h"

// TODO: added because of multiple definitions of fmt::v11::detail::assert_fail
#  if defined(_WIN32) && !defined(FMT_ASSERT)
#    define FMT_ASSERT
#  endif

#  include <KDGpu/instance.h>
#  include <KDGpu/device.h>
#  include <KDGpu/queue.h>
#  include <vulkan/vulkan.h>
#  include <ktxvulkan.h>
#  include <memory>

namespace KDGpu {
    class Adapter;
    class Surface;
    class VulkanGraphicsApi;
} // namespace KDGpu

class VKDevice final : public Device
{
public:
    VKDevice();
    ~VKDevice() override;

    bool initialize(const AdapterIdentity &adapterIdentity) override;
    bool initialize(KDGpu::Surface &surface,
        const AdapterIdentity &adapterIdentity);
    void shutdown() override;
    bool isValid() const override { return mDevice.isValid(); }

    bool hasAdapters();

    KDGpu::Instance &instance();
    KDGpu::Adapter &adapter();
    KDGpu::Device &device();
    KDGpu::Queue &queue();
    ktxVulkanDeviceInfo &ktxDeviceInfo();

private:
    bool ensureInstance();
    bool chooseQueue(KDGpu::Surface *surface, bool computeRequired);
    bool initializeKtx();
    void releaseKtx();
    void error(MessageType messageType, const QString &message = {});

    std::unique_ptr<KDGpu::VulkanGraphicsApi> mApi;
    KDGpu::Instance mInstance;
    KDGpu::Adapter *mAdapter{};
    KDGpu::Device mDevice;
    KDGpu::Queue mQueue;
    ktxVulkanDeviceInfo mKtxDeviceInfo{};
    VkCommandPool mKtxCommandPool{};
    bool mKtxDeviceInfoInitialized{};
    MessagePtrSet mMessages;
};

std::shared_ptr<VKDevice> sharedVKDevice();
void resetSharedVKDevice();

#endif // defined(VULKAN_ENABLED)
