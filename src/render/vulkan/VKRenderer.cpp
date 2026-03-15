#include "VKRenderer.h"
#include "MessageList.h"
#include "TextureData.h"
#include "render/AdapterIdentity.h"
#include "render/RenderTask.h"
#include <QApplication>
#include <QMutex>
#include <QSemaphore>

// TODO: added because of multiple definitions of fmt::v11::detail::assert_fail
#if defined(_WIN32) && !defined(FMT_ASSERT)
#  define FMT_ASSERT
#endif

#include <KDGpu/device.h>
#include <KDGpu/instance.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>

#if defined(_WIN32)
#  define NOMINMAX
#  define WIN32_LEAN_AND_MEAN
#  include <spdlog/sinks/msvc_sink.h>
#  include <vulkan/vulkan_win32.h>
#endif

namespace {
    AdapterIdentity gAdapterIdentity;
} // namespace

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
            shutdown();
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
        const auto error = [&](const QString &message) {
            mMessages += MessageList::insert(0, MessageType::VulkanNotAvailable,
                message);
            shutdown();
        };

#if defined(_WIN32)
        static auto l =
            spdlog::synchronous_factory::create<spdlog::sinks::msvc_sink_mt>(
                "KDGpu");
#endif

        mApi = std::make_unique<KDGpu::VulkanGraphicsApi>();

        const auto version = qApp->applicationVersion();
        auto major = 0;
        auto minor = 0;
        auto build = 0;
        std::sscanf(qPrintable(version), "%d.%d.%d", &major, &minor, &build);

        const auto instanceOptions = KDGpu::InstanceOptions{
            .applicationName = qApp->applicationName().toStdString(),
            .applicationVersion =
                KDGPU_MAKE_API_VERSION(0, major, minor, build),
#if !defined(NDEBUG)
            .layers = { "VK_LAYER_KHRONOS_validation" },
#endif
        };
        mInstance = mApi->createInstance(instanceOptions);
        if (!mInstance.isValid() || mInstance.adapters().empty())
            return error("");

        mAdapter = [&]() -> KDGpu::Adapter * {
            for (auto adapter : mInstance.adapters()) {
                if (std::ranges::equal(adapter->properties().driverUUID,
                        gAdapterIdentity.driverUUID))
                    for (const auto &deviceUUID : gAdapterIdentity.deviceUUIDs)
                        if (std::ranges::equal(adapter->properties().deviceUUID,
                                deviceUUID))
                            return adapter;
            }
            return nullptr;
        }();
        if (!mAdapter)
            return error("no adapter found");

        KDGpu::Logger::logger()->info("Initializing Vulkan on the "
            + adapterDeviceTypeToString(mAdapter->properties().deviceType) + " "
            + mAdapter->properties().deviceName);

        mDevice = mAdapter->createDevice(KDGpu::DeviceOptions{
            .requestedFeatures = mAdapter->features(),
        });
        if (!mDevice.isValid())
            return error("creating device failed");

        const auto requiredQueueFlags =
            KDGpu::QueueFlags(KDGpu::QueueFlagBits::GraphicsBit
                | KDGpu::QueueFlagBits::TransferBit
                | KDGpu::QueueFlagBits::ComputeBit);
        for (const auto &queue : mDevice.queues())
            if ((queue.flags() & requiredQueueFlags) == requiredQueueFlags) {
                mQueue = queue;
                break;
            }
        if (!mQueue.isValid())
            return error("no general queue found");

        const auto &rm = *mDevice.graphicsApi()->resourceManager();
        const auto vkInstance = rm.getInstance(mInstance);
        const auto vkAdapter = rm.getAdapter(*mAdapter);
        const auto vkDevice = rm.getDevice(mDevice);
        const auto vkQueue = rm.getQueue(mQueue);

        const auto commandPoolInfo = VkCommandPoolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        };
        if (vkCreateCommandPool(vkDevice->device, &commandPoolInfo, nullptr,
                &mKtxCommandPool)
            != VK_SUCCESS)
            return error("creating command pool failed");

        if (ktxVulkanDeviceInfo_ConstructEx(&mKtxDeviceInfo,
                vkInstance->instance, vkAdapter->physicalDevice,
                vkDevice->device, vkQueue->queue, mKtxCommandPool, nullptr,
                nullptr)
            != KTX_SUCCESS)
            return error("creating device info for KTX failed");

        mRenderer.mDevice = &mDevice;
        mRenderer.mQueue = &mQueue;
        mRenderer.mKtxDeviceInfo = &mKtxDeviceInfo;
    }

    void shutdown()
    {
        if (mDevice.isValid()) {
            ktxVulkanDeviceInfo_Destruct(&mKtxDeviceInfo);

            if (mKtxCommandPool) {
                const auto &rm = *mDevice.graphicsApi()->resourceManager();
                const auto vkDevice =
                    static_cast<KDGpu::VulkanDevice *>(rm.getDevice(mDevice));
                vkDestroyCommandPool(vkDevice->device, mKtxCommandPool,
                    nullptr);
            }
        }
        mQueue = {};
        mDevice = {};
        mInstance = {};
        mApi.reset();

        mRenderer.mDevice = nullptr;
        mRenderer.mQueue = nullptr;
        mRenderer.mKtxDeviceInfo = nullptr;
        mRenderer.setFailed();
    }

    VKRenderer &mRenderer;
    bool mInitialized{};
    std::unique_ptr<KDGpu::VulkanGraphicsApi> mApi;
    KDGpu::Instance mInstance;
    KDGpu::Adapter *mAdapter{};
    KDGpu::Device mDevice;
    KDGpu::Queue mQueue;
    ktxVulkanDeviceInfo mKtxDeviceInfo{};
    VkCommandPool mKtxCommandPool;
    MessagePtrSet mMessages;
};

VKRenderer::VKRenderer(QObject *parent)
    : QObject(parent)
    , Renderer(RenderAPI::Vulkan)
    , mWorker(std::make_unique<Worker>(this))
{
    gAdapterIdentity = getOpenGLAdapterIdentity();

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

KDGpu::Queue &VKRenderer::queue()
{
    Q_ASSERT(mQueue);
    return *mQueue;
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
