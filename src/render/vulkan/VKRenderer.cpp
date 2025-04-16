#include "VKRenderer.h"
#include "MessageList.h"
#include "TextureData.h"
#include "render/RenderTask.h"
#include <QApplication>
#include <QMutex>
#include <QSemaphore>

#include <KDGpu/device.h>
#include <KDGpu/instance.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>

#if defined(KDGPU_PLATFORM_WIN32)
#  define NOMINMAX
#  define WIN32_LEAN_AND_MEAN
#  include <spdlog/sinks/msvc_sink.h>
#  include <vulkan/vulkan_win32.h>
#endif

class VKRenderer::Worker final : public QObject
{
    Q_OBJECT

public:
    explicit Worker(VKRenderer *renderer) : mRenderer(*renderer) { }

    void handleConfigureTask(RenderTask *renderTask)
    {
        try {
            if (!std::exchange(mInitialized, true))
                initialize();

            if (mDevice.isValid())
                renderTask->configure();
        } catch (const std::exception &ex) {
            mMessages +=
                MessageList::insert(0, MessageType::RenderingFailed, ex.what());
        }
        Q_EMIT taskConfigured();
    }

    void handleRenderTask(RenderTask *renderTask)
    {
        try {
            if (mDevice.isValid())
                renderTask->render();
        } catch (const std::exception &ex) {
            mMessages +=
                MessageList::insert(0, MessageType::RenderingFailed, ex.what());
        }
        Q_EMIT taskRendered();
    }

    void handleReleaseTask(RenderTask *renderTask, void *userData)
    {
        if (mDevice.isValid())
            renderTask->release();
        static_cast<QSemaphore *>(userData)->release(1);
    }

public Q_SLOTS:
    void stop()
    {
        if (mDevice.isValid())
            shutdown();
        QThread::currentThread()->exit(0);
    }

Q_SIGNALS:
    void taskConfigured();
    void taskRendered();

private:
    void initialize()
    {
#if defined(KDGPU_PLATFORM_WIN32)
        static auto l =
            spdlog::synchronous_factory::create<spdlog::sinks::msvc_sink_mt>(
                "KDGpu");
#endif

        mApi = std::make_unique<KDGpu::VulkanGraphicsApi>();

        auto instanceOptions = KDGpu::InstanceOptions
        {
#if !defined(NDEBUG)
            .layers = { "VK_LAYER_KHRONOS_validation" },
#endif
            .extensions = {
                VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
                VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME
            }
        };
        mInstance = mApi->createInstance(instanceOptions);
        if (!mInstance.isValid() || mInstance.adapters().empty()) {
            mMessages +=
                MessageList::insert(0, MessageType::VulkanNotAvailable);
            return;
        }

        mAdapter = mInstance.selectAdapter(KDGpu::AdapterDeviceType::Default);

        auto deviceOptions =
            KDGpu::DeviceOptions{ .requestedFeatures = mAdapter->features() };
        deviceOptions.extensions = {
            VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
#if defined(KDGPU_PLATFORM_WIN32)
            VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
#endif
        };
        mDevice = mAdapter->createDevice(deviceOptions);

        auto &rm = *mDevice.graphicsApi()->resourceManager();
        auto vkInstance =
            static_cast<KDGpu::VulkanInstance *>(rm.getInstance(mInstance));
        auto vkAdapter =
            static_cast<KDGpu::VulkanAdapter *>(rm.getAdapter(*mAdapter));
        auto vkDevice =
            static_cast<KDGpu::VulkanDevice *>(rm.getDevice(mDevice));
        auto vkQueue =
            static_cast<KDGpu::VulkanQueue *>(rm.getQueue(mDevice.queues()[0]));

        auto poolInfo = VkCommandPoolCreateInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = 0;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCreateCommandPool(vkDevice->device, &poolInfo, nullptr,
            &mKtxCommandPool);

        ktxVulkanDeviceInfo_ConstructEx(&mKtxDeviceInfo, vkInstance->instance,
            vkAdapter->physicalDevice, vkDevice->device, vkQueue->queue,
            mKtxCommandPool, nullptr, nullptr);

        mRenderer.mDevice = &mDevice;
        mRenderer.mKtxDeviceInfo = &mKtxDeviceInfo;
    }

    void shutdown()
    {
        ktxVulkanDeviceInfo_Destruct(&mKtxDeviceInfo);

        auto &rm = *mDevice.graphicsApi()->resourceManager();
        auto vkDevice =
            static_cast<KDGpu::VulkanDevice *>(rm.getDevice(mDevice));
        vkDestroyCommandPool(vkDevice->device, mKtxCommandPool, nullptr);

        mRenderer.mDevice = nullptr;
        mRenderer.mKtxDeviceInfo = nullptr;
    }

    VKRenderer &mRenderer;
    bool mInitialized{};
    std::unique_ptr<KDGpu::VulkanGraphicsApi> mApi;
    KDGpu::Instance mInstance;
    KDGpu::Adapter *mAdapter{};
    KDGpu::Device mDevice;
    ktxVulkanDeviceInfo mKtxDeviceInfo{};
    VkCommandPool mKtxCommandPool;
    MessagePtrSet mMessages;
};

VKRenderer::VKRenderer(QObject *parent)
    : QObject(parent)
    , Renderer(RenderAPI::Vulkan)
    , mWorker(std::make_unique<Worker>(this))
{
    mWorker->moveToThread(&mThread);

    connect(this, &VKRenderer::configureTask, mWorker.get(),
        &Worker::handleConfigureTask);
    connect(mWorker.get(), &Worker::taskConfigured, this,
        &VKRenderer::handleTaskConfigured);
    connect(this, &VKRenderer::renderTask, mWorker.get(),
        &Worker::handleRenderTask);
    connect(mWorker.get(), &Worker::taskRendered, this,
        &VKRenderer::handleTaskRendered);
    connect(this, &VKRenderer::releaseTask, mWorker.get(),
        &Worker::handleReleaseTask);

    mThread.start();
}

VKRenderer::~VKRenderer()
{
    mPendingTasks.clear();

    QMetaObject::invokeMethod(mWorker.get(), "stop", Qt::QueuedConnection);
    mThread.wait();

    mWorker.reset();
}

void VKRenderer::render(RenderTask *task)
{
    Q_ASSERT(!mPendingTasks.contains(task));
    mPendingTasks.append(task);

    renderNextTask();
}

void VKRenderer::release(RenderTask *task)
{
    mPendingTasks.removeAll(task);
    while (mCurrentTask == task)
        qApp->processEvents(QEventLoop::WaitForMoreEvents);

    QSemaphore done(1);
    done.acquire(1);
    Q_EMIT releaseTask(task, &done, QPrivateSignal());

    // block until the render thread finished releasing the task
    done.acquire(1);
    done.release(1);
}

KDGpu::Device &VKRenderer::device()
{
    Q_ASSERT(mDevice);
    return *mDevice;
}

ktxVulkanDeviceInfo &VKRenderer::ktxDeviceInfo()
{
    Q_ASSERT(mKtxDeviceInfo);
    return *mKtxDeviceInfo;
}

void VKRenderer::renderNextTask()
{
    if (mCurrentTask || mPendingTasks.isEmpty())
        return;

    mCurrentTask = mPendingTasks.takeFirst();
    Q_EMIT configureTask(mCurrentTask, QPrivateSignal());
}

void VKRenderer::handleTaskConfigured()
{
    mCurrentTask->configured();

    Q_EMIT renderTask(mCurrentTask, QPrivateSignal());
}

void VKRenderer::handleTaskRendered()
{
    auto currentTask = std::exchange(mCurrentTask, nullptr);
    currentTask->handleRendered();

    renderNextTask();
}

#include "VKRenderer.moc"
