#pragma once
#if defined(VULKAN_ENABLED)

#include <KDGpu/gpu_core.h>
#include "render/AdapterIdentity.h"
#include "render/RenderWindow.h"
#include <QList>
#include <memory>

struct ktxVulkanDeviceInfo;
struct VKContext;

namespace KDGpu {
    class Device;
    class Queue;
    class RenderPassCommandRecorder;
}

class VKWindow : public RenderWindow
{
    Q_OBJECT
public:
    static bool isSupported();
    static QList<AdapterIdentity> getAdapterIdentities();

    explicit VKWindow(int syncInterval = 0);
    ~VKWindow() override;

    bool initialized() const override { return mInitialized; }
    KDGpu::Device &device();
    KDGpu::Queue &queue();
    ktxVulkanDeviceInfo &ktxDeviceInfo();
    KDGpu::RenderPassCommandRecorder &renderPass();
    KDGpu::Format swapchainFormat() const;
    KDGpu::Extent2D swapchainExtent() const;

    void update() override;

private:
    struct State;

    void releaseGpu();
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    void initializeGpu();
    void paintGpu();
    void swapBuffers();

    const int mSyncInterval{};
    bool mInitialized{};
    std::unique_ptr<State> mState;
};

#endif // defined(VULKAN_ENABLED)
