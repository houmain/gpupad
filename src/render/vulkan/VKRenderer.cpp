#include "VKRenderer.h"
#include "../RenderTask.h"
#include <QApplication>
#include <QSemaphore>
#include <QMutex>

#include <KDGpu/device.h>
#include <KDGpu/instance.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>

#if defined(KDGPU_PLATFORM_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <vulkan/vulkan_win32.h>
#endif

class VKRenderer::Worker final : public QObject
{
    Q_OBJECT

public:
    explicit Worker(VKRenderer *renderer)
        : mRenderer(*renderer)
    {
    }

    void handleConfigureTask(RenderTask* renderTask)
    {
        renderTask->configure();
        Q_EMIT taskConfigured();
    }

    void handleRenderTask(RenderTask* renderTask)
    {
        if (!std::exchange(mInitialized, true))
            initialize();

        renderTask->render();
        Q_EMIT taskRendered();
    }

    void handleReleaseTask(RenderTask* renderTask, void* userData)
    {
        renderTask->release();
        static_cast<QSemaphore*>(userData)->release(1);
    }

public Q_SLOTS:
    void stop()
    {
        mRenderer.mDevice = nullptr;
        QThread::currentThread()->exit(0);
    }

Q_SIGNALS:
    void taskConfigured();
    void taskRendered();

private:
    void initialize()
    {
        mApi = std::make_unique<KDGpu::VulkanGraphicsApi>();

        auto instanceOptions = KDGpu::InstanceOptions{
#if !defined(NDEBUG)
          .layers = {
            "VK_LAYER_KHRONOS_validation"
          },
#endif
          .extensions = {
            VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME
          }
        };
        mInstance = mApi->createInstance(instanceOptions);

        mAdapter = mInstance.selectAdapter(KDGpu::AdapterDeviceType::Default);

        auto deviceOptions = KDGpu::DeviceOptions{ .requestedFeatures = mAdapter->features() };
        deviceOptions.extensions = {
            VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
#if defined(KDGPU_PLATFORM_WIN32)
            VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
#endif
        };
        mDevice = mAdapter->createDevice(deviceOptions);
        mRenderer.mDevice = &mDevice;
    }

    VKRenderer &mRenderer;
    bool mInitialized{ };
    std::unique_ptr<KDGpu::VulkanGraphicsApi> mApi;
    KDGpu::Instance mInstance;
    KDGpu::Adapter *mAdapter{ };
    KDGpu::Device mDevice;
};

VKRenderer::VKRenderer(QObject *parent)
    : QObject(parent)
    , Renderer(RenderAPI::Vulkan)
    , mWorker(new Worker(this))
{
    mWorker->moveToThread(&mThread);

    connect(this, &VKRenderer::configureTask,
        mWorker.data(), &Worker::handleConfigureTask);
    connect(mWorker.data(), &Worker::taskConfigured,
        this, &VKRenderer::handleTaskConfigured);
    connect(this, &VKRenderer::renderTask,
        mWorker.data(), &Worker::handleRenderTask);
    connect(mWorker.data(), &Worker::taskRendered,
        this, &VKRenderer::handleTaskRendered);
    connect(this, &VKRenderer::releaseTask,
        mWorker.data(), &Worker::handleReleaseTask);

    mThread.start();
}

VKRenderer::~VKRenderer()
{
    mPendingTasks.clear();
    
    QMetaObject::invokeMethod(mWorker.data(), "stop", Qt::QueuedConnection);
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
