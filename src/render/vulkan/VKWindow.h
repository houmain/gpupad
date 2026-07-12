#pragma once
#if defined(VULKAN_ENABLED)

#  include "VKDevice.h"
#  include <QWindow>
#  include <KDGpu/gpu_core.h>
#  include "VKContext.h"
#  include "render/AdapterIdentity.h"
#  include <QList>
#  include <memory>

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

    bool initialized() const { return static_cast<bool>(mState); }
    VKDevice &device();
    VKDevice::Lock lockDevice();
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

    void initializeGpu();
    void releaseGpu();
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    bool ensureSwapchain();

    const bool mEnableVSync;
    std::unique_ptr<State> mState;
};

#endif // defined(VULKAN_ENABLED)
