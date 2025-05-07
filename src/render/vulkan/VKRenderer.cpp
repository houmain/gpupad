#include "VKRenderer.h"
#include "MessageList.h"
#include "TextureData.h"
#include "render/RenderTask.h"
#include <QApplication>
#include <QMutex>
#include <QSemaphore>
#include <QOpenGLContext>
#include <QOffscreenSurface>

#include <KDGpu/device.h>
#include <KDGpu/instance.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>

#if defined(KDGPU_PLATFORM_WIN32)
#  define NOMINMAX
#  define WIN32_LEAN_AND_MEAN
#  include <spdlog/sinks/msvc_sink.h>
#  include <vulkan/vulkan_win32.h>
#endif

using UUID = std::array<uint8_t, 16>;

namespace {
    struct AdapterIdentity
    {
        UUID deviceUUIDs[4];
        UUID driverUUID;
    };

    AdapterIdentity gAdapterIdentity;

    AdapterIdentity getOpenGLAdapterIdentity()
    {
        auto glContext = QOpenGLContext();
        glContext.setShareContext(QOpenGLContext::globalShareContext());
        auto surface = QOffscreenSurface();
        surface.setFormat(glContext.format());
        surface.create();
        glContext.create();
        if (!glContext.makeCurrent(&surface))
            return {};
        const auto guard = QScopeGuard([&]() { glContext.doneCurrent(); });

        if (!glContext.hasExtension("GL_EXT_memory_object"))
            return {};
        const auto glGetUnsignedBytevEXT =
            reinterpret_cast<PFNGLGETUNSIGNEDBYTEVEXTPROC>(
                glContext.getProcAddress("glGetUnsignedBytevEXT"));
        if (!glGetUnsignedBytevEXT)
            return {};

        const auto glGetUnsignedBytei_vEXT =
            reinterpret_cast<PFNGLGETUNSIGNEDBYTEI_VEXTPROC>(
                glContext.getProcAddress("glGetUnsignedBytei_vEXT"));
        if (!glGetUnsignedBytei_vEXT)
            return {};

        auto identity = AdapterIdentity();
        static_assert(GL_UUID_SIZE_EXT == sizeof(UUID));
        auto numDeviceUuids = GLint{};
        glGetIntegerv(GL_NUM_DEVICE_UUIDS_EXT, &numDeviceUuids);
        for (auto i = 0; i < std::min(numDeviceUuids, 4); ++i)
            glGetUnsignedBytei_vEXT(GL_DEVICE_UUID_EXT, i,
                identity.deviceUUIDs[i].data());
        glGetUnsignedBytevEXT(GL_DRIVER_UUID_EXT, identity.driverUUID.data());
        return identity;
    }
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

#if defined(KDGPU_PLATFORM_WIN32)
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

        const auto instanceOptions = KDGpu::InstanceOptions
        {
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
        const auto vkInstance =
            static_cast<KDGpu::VulkanInstance *>(rm.getInstance(mInstance));
        const auto vkAdapter =
            static_cast<KDGpu::VulkanAdapter *>(rm.getAdapter(*mAdapter));
        const auto vkDevice =
            static_cast<KDGpu::VulkanDevice *>(rm.getDevice(mDevice));
        const auto vkQueue =
            static_cast<KDGpu::VulkanQueue *>(rm.getQueue(mQueue));

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
