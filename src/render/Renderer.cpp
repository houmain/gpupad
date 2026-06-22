#include "Renderer.h"
#include "MessageList.h"
#include "RenderTask.h"
#include <QApplication>
#include <QSemaphore>

class Renderer::Worker final : public QObject
{
    Q_OBJECT

public:
    Worker(Renderer &renderer, std::unique_ptr<Device> device,
        const AdapterIdentity &adapterIdentity)
        : mRenderer(renderer)
        , mDevice(std::move(device))
        , mAdapterIdentity(adapterIdentity)
    {
    }

    Device *device() const { return mDevice.get(); }

    void handleConfigureTask(RenderTask *renderTask)
    {
        try {
            if (!std::exchange(mInitialized, true)
                && !mDevice->initialize(mAdapterIdentity)) {
                mRenderer.setFailed();
                mDevice.reset();
            }
            if (mDevice)
                mRenderer.configureRenderTask(renderTask);
        } catch (const std::exception &ex) {
            mMessages.insert(0, MessageType::RenderingFailed, ex.what());
        }
        Q_EMIT taskConfigured();
    }

    void handleRenderTask(RenderTask *renderTask)
    {
        try {
            if (mDevice)
                mRenderer.renderRenderTask(renderTask);
        } catch (const std::exception &ex) {
            mMessages.insert(0, MessageType::RenderingFailed, ex.what());
        }
        Q_EMIT taskRendered();
    }

    void handleReleaseTask(RenderTask *renderTask, void *userData)
    {
        if (mDevice)
            mRenderer.releaseRenderTask(renderTask);
        static_cast<QSemaphore *>(userData)->release(1);
    }

public Q_SLOTS:
    void stop()
    {
        mDevice.reset();
        QThread::currentThread()->exit(0);
    }

Q_SIGNALS:
    void taskConfigured();
    void taskRendered();

private:
    Renderer &mRenderer;
    std::unique_ptr<Device> mDevice;
    const AdapterIdentity mAdapterIdentity;
    bool mInitialized{};
    MessagePtrSet mMessages;
};

Renderer::Renderer(Type type, MessageType failureMessage, QObject *parent)
    : QObject(parent)
    , mType(type)
{
    mMessages.insert(0, failureMessage);
    setFailed();
}

Renderer::Renderer(Type type, std::unique_ptr<Device> device,
    const AdapterIdentity &adapterIdentity, QObject *parent)
    : QObject(parent)
    , mType(type)
{
    Q_ASSERT(device);
    device->moveToThread(&mThread);
    mWorker =
        std::make_unique<Worker>(*this, std::move(device), adapterIdentity);
    mWorker->moveToThread(&mThread);

    connect(this, &Renderer::configureTaskRequested, mWorker.get(),
        &Worker::handleConfigureTask);
    connect(mWorker.get(), &Worker::taskConfigured, this,
        &Renderer::handleTaskConfigured);
    connect(this, &Renderer::renderTaskRequested, mWorker.get(),
        &Worker::handleRenderTask);
    connect(mWorker.get(), &Worker::taskRendered, this,
        &Renderer::handleTaskRendered);
    connect(this, &Renderer::releaseTaskRequested, mWorker.get(),
        &Worker::handleReleaseTask);

    mThread.start();
}

Renderer::~Renderer()
{
    if (!mWorker)
        return;

    mPendingTasks.clear();

    QMetaObject::invokeMethod(
        mWorker.get(), [worker = mWorker.get()]() { worker->stop(); },
        Qt::QueuedConnection);
    mThread.wait();

    mWorker.reset();
}

QThread *Renderer::renderThread()
{
    return mWorker ? &mThread : nullptr;
}

void Renderer::finish()
{
    for (auto i = 0; i < 100; ++i) {
        if (!mCurrentTask)
            return;
        qApp->processEvents(QEventLoop::WaitForMoreEvents);
    }
    Q_ASSERT(!"unreachable");
}

Device &Renderer::device()
{
    Q_ASSERT(mWorker && mWorker->device());
    return *mWorker->device();
}

const Device &Renderer::device() const
{
    Q_ASSERT(mWorker && mWorker->device());
    return *mWorker->device();
}

void Renderer::render(RenderTask *task)
{
    if (!mWorker)
        return;

    Q_ASSERT(!mPendingTasks.contains(task));
    mPendingTasks.append(task);

    renderNextTask();
}

void Renderer::release(RenderTask *task)
{
    if (!mWorker)
        return;

    mPendingTasks.removeAll(task);
    finish();

    QSemaphore done(1);
    done.acquire(1);
    Q_EMIT releaseTaskRequested(task, &done, QPrivateSignal());

    // block until the render thread finished releasing the task
    done.acquire(1);
    done.release(1);
}

void Renderer::renderNextTask()
{
    if (mCurrentTask || mPendingTasks.isEmpty())
        return;

    mCurrentTask = mPendingTasks.takeFirst();
    Q_EMIT configureTaskRequested(mCurrentTask, QPrivateSignal());
}

void Renderer::handleTaskConfigured()
{
    configuredRenderTask(mCurrentTask);

    Q_EMIT renderTaskRequested(mCurrentTask, QPrivateSignal());
}

void Renderer::handleTaskRendered()
{
    auto currentTask = std::exchange(mCurrentTask, nullptr);
    finishRenderTask(currentTask);

    renderNextTask();
}

void Renderer::configureRenderTask(RenderTask *task)
{
    task->configure();
}

void Renderer::renderRenderTask(RenderTask *task)
{
    task->render();
}

void Renderer::releaseRenderTask(RenderTask *task)
{
    task->release();
}

void Renderer::configuredRenderTask(RenderTask *task)
{
    task->configured();
}

void Renderer::finishRenderTask(RenderTask *task)
{
    task->handleRendered();
}

#include "Renderer.moc"
