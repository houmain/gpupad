#pragma once

#include "render/Renderer.h"
#include <QObject>
#include <QThread>

namespace KDGpu {
    class Device;
    class Queue;
}
struct ktxVulkanDeviceInfo;

class VKRenderer : public QObject, public Renderer
{
    Q_OBJECT
public:
#if defined(VULKAN_ENABLED)
    explicit VKRenderer(QObject *parent = nullptr);
    ~VKRenderer() override;

    void render(RenderTask *task) override;
    void release(RenderTask *task) override;
    void finish() override;
    QThread *renderThread() override { return &mThread; }

    KDGpu::Device &device();
    KDGpu::Queue &queue();
    ktxVulkanDeviceInfo &ktxDeviceInfo();

Q_SIGNALS:
    void configureTask(RenderTask *renderTask, QPrivateSignal);
    void renderTask(RenderTask *renderTask, QPrivateSignal);
    void releaseTask(RenderTask *renderTask, void *userData, QPrivateSignal);

private:
    class Worker;
    void handleTaskConfigured();
    void handleTaskRendered();
    void renderNextTask();

    QThread mThread;
    std::unique_ptr<Worker> mWorker;
    QList<RenderTask *> mPendingTasks;
    RenderTask *mCurrentTask{};
    KDGpu::Device *mDevice{};
    KDGpu::Queue *mQueue{};
    ktxVulkanDeviceInfo *mKtxDeviceInfo{};

#else
public:
    explicit VKRenderer(QObject *parent = nullptr)
        : QObject(parent)
        , Renderer(Renderer::Type::Vulkan)
    {
        mMessages.insert(0, MessageType::VulkanNotAvailable);
        setFailed();
    }

    ~VKRenderer() = default;
    void render(RenderTask *task) override { }
    void release(RenderTask *task) override { }
    void finish() override { }
    QThread *renderThread() override { return nullptr; }

private:
    MessagePtrSet mMessages;

#endif // defined(VULKAN_ENABLED)
};
