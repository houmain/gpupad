#pragma once
#if defined(VULKAN_ENABLED)

#  include "VKDevice.h"
#  include <QWindow>
#  include <KDGpu/gpu_core.h>
#  include "VKContext.h"
#  include "render/AdapterIdentity.h"
#  include <QList>
#  include <memory>
#  include <optional>

struct ktxVulkanDeviceInfo;

class VKWindow : public QWindow
{
    Q_OBJECT
public:
    static bool isSupported();
    static QList<AdapterIdentity> getAdapterIdentities();

    explicit VKWindow(bool enableVSync = false, QWindow *parent = nullptr);
    explicit VKWindow(QWindow *parent) = delete;
    ~VKWindow() override;

    bool initializeGpu();
    bool initialized() const { return static_cast<bool>(mState); }
    VKDevice &device();
    VKDevice::Lock lockDevice();
    VKContext &context();
    VKDevice::Lock beginCommandQueue();
    void submitCommandQueueWaitIdle();
    KDGpu::RenderPassCommandRecorder &renderPass();
    KDGpu::Format swapchainFormat() const;
    KDGpu::Extent2D swapchainExtent() const;

    void redraw();

Q_SIGNALS:
    void initializingGpu();
    void preparingGpu();
    void paintingGpu();
    void submittedGpu();
    void releasingGpu();

private:
    struct State;

    void releaseGpu();
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    bool ensureSwapchain(VKDevice::Lock &deviceLock);
    void submitCommandQueue(bool waitUntilIdle, KDGpu::SubmitOptions options);

    const bool mEnableVSync;
    bool mRedrawing{};
    std::unique_ptr<State> mState;
    std::optional<VKContext> mContext;
};

#endif // defined(VULKAN_ENABLED)
