#include "D3DRenderer.h"
#include "D3DContext.h"
#include "TextureData.h"
#include "render/AdapterIdentity.h"
#include "render/RenderTask.h"
#include "d3d12/RenderTargetHelper.hpp"
#include <QApplication>
#include <QMutex>
#include <QSemaphore>

namespace {
    AdapterIdentity gAdapterIdentity;
} // namespace

class D3DRenderer::Worker final : public QObject
{
    Q_OBJECT

public:
    explicit Worker(D3DRenderer *renderer) : mRenderer(*renderer) { }

    void handleConfigureTask(RenderTask *renderTask)
    {
        try {
            if (!std::exchange(mInitialized, true))
                initialize();

            if (mDevice)
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
            if (mDevice)
                renderTask->render();
        } catch (const std::exception &ex) {
            mMessages +=
                MessageList::insert(0, MessageType::RenderingFailed, ex.what());
        }
        Q_EMIT taskRendered();
    }

    void handleReleaseTask(RenderTask *renderTask, void *userData)
    {
        if (mDevice)
            renderTask->release();
        static_cast<QSemaphore *>(userData)->release(1);
    }

public Q_SLOTS:
    void stop()
    {
        if (mDevice)
            shutdown();
        QThread::currentThread()->exit(0);
    }

Q_SIGNALS:
    void taskConfigured();
    void taskRendered();

private:
    void initialize() noexcept
    try {
        const auto ThrowIfFailed = [](HRESULT hr) {
            if (FAILED(hr))
                throw std::runtime_error("failed");
        };

#if !defined(NDEBUG)
        auto debugController = ComPtr<ID3D12Debug>();
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
        debugController->EnableDebugLayer();
#endif

        ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mDxgiFactory)));

        mAdapter = [&]() -> IDXGIAdapter * {
            auto i = UINT{};
            auto adapter = std::add_pointer_t<IDXGIAdapter>{};
            while (mDxgiFactory->EnumAdapters(i, &adapter)
                != DXGI_ERROR_NOT_FOUND) {
                auto desc = DXGI_ADAPTER_DESC{};
                adapter->GetDesc(&desc);
                if (!std::memcmp(&desc.AdapterLuid,
                        &gAdapterIdentity.deviceLUID, sizeof(LUID)))
                    return adapter;
                ++i;
            }
            return nullptr;
        }();

        ThrowIfFailed(D3D12CreateDevice(mAdapter, D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&mDevice)));

        auto queueDesc = D3D12_COMMAND_QUEUE_DESC{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        ThrowIfFailed(
            mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mQueue)));

        ThrowIfFailed(mRenderTargetHelper.Init(mDevice.Get()));

        mRenderer.mDevice = mDevice.Get();
        mRenderer.mQueue = mQueue.Get();
        mRenderer.mRenderTargetHelper = &mRenderTargetHelper;

    } catch (const std::exception &) {
        mMessages += MessageList::insert(0, MessageType::Direct3DNotAvailable);
        shutdown();
    }

    void shutdown()
    {
        mRenderTargetHelper.Destroy();
        mCommandAllocator.Reset();
        mQueue.Reset();
        mDevice.Reset();
        mAdapter = {};
        mDxgiFactory.Reset();

        mRenderer.mDevice = nullptr;
        mRenderer.mQueue = nullptr;
        mRenderer.mRenderTargetHelper = nullptr;
    }

    D3DRenderer &mRenderer;
    bool mInitialized{};
    ComPtr<IDXGIFactory4> mDxgiFactory;
    IDXGIAdapter *mAdapter{};
    ComPtr<ID3D12Device> mDevice;
    ComPtr<ID3D12CommandQueue> mQueue;
    ComPtr<ID3D12CommandAllocator> mCommandAllocator;
    RenderTargetHelper mRenderTargetHelper;
    MessagePtrSet mMessages;
};

D3DRenderer::D3DRenderer(QObject *parent)
    : QObject(parent)
    , Renderer(RenderAPI::Direct3D)
    , mWorker(std::make_unique<Worker>(this))
{
    gAdapterIdentity = getOpenGLAdapterIdentity();

    mWorker->moveToThread(&mThread);

    connect(this, &D3DRenderer::configureTask, mWorker.get(),
        &Worker::handleConfigureTask);
    connect(mWorker.get(), &Worker::taskConfigured, this,
        &D3DRenderer::handleTaskConfigured);
    connect(this, &D3DRenderer::renderTask, mWorker.get(),
        &Worker::handleRenderTask);
    connect(mWorker.get(), &Worker::taskRendered, this,
        &D3DRenderer::handleTaskRendered);
    connect(this, &D3DRenderer::releaseTask, mWorker.get(),
        &Worker::handleReleaseTask);

    mThread.start();
}

D3DRenderer::~D3DRenderer()
{
    mPendingTasks.clear();

    QMetaObject::invokeMethod(mWorker.get(), "stop", Qt::QueuedConnection);
    mThread.wait();

    mWorker.reset();
}

void D3DRenderer::render(RenderTask *task)
{
    Q_ASSERT(!mPendingTasks.contains(task));
    mPendingTasks.append(task);

    renderNextTask();
}

void D3DRenderer::release(RenderTask *task)
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

ID3D12Device &D3DRenderer::device()
{
    Q_ASSERT(mDevice);
    return *mDevice;
}

ID3D12CommandQueue &D3DRenderer::queue()
{
    Q_ASSERT(mQueue);
    return *mQueue;
}

RenderTargetHelper &D3DRenderer::renderTargetHelper()
{
    Q_ASSERT(mRenderTargetHelper);
    return *mRenderTargetHelper;
}

void D3DRenderer::renderNextTask()
{
    if (mCurrentTask || mPendingTasks.isEmpty())
        return;

    mCurrentTask = mPendingTasks.takeFirst();
    Q_EMIT configureTask(mCurrentTask, QPrivateSignal());
}

void D3DRenderer::handleTaskConfigured()
{
    mCurrentTask->configured();

    Q_EMIT renderTask(mCurrentTask, QPrivateSignal());
}

void D3DRenderer::handleTaskRendered()
{
    auto currentTask = std::exchange(mCurrentTask, nullptr);
    currentTask->handleRendered();

    renderNextTask();
}

#include "D3DRenderer.moc"
