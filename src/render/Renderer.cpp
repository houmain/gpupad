#include "Renderer.h"
#include "RenderTask.h"
#include "GLContext.h"
#include <QApplication>
#include <QOpenGLFunctions>
#include <QOffscreenSurface>
#include <QOpenGLDebugLogger>
#include <QSemaphore>
#include <QMutex>

namespace {
    QMutex gFirstGLErrorMutex;
    QString gFirstGLError;
}

QString getFirstGLError()
{
    auto lock = QMutexLocker(&gFirstGLErrorMutex);
    return std::exchange(gFirstGLError, "");
}

class Renderer::Worker final : public QObject
{
    Q_OBJECT

public:
    GLContext context;
    QOffscreenSurface surface;

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
        mDebugLogger.reset();
        context.doneCurrent();

        auto guiThread = QApplication::instance()->thread();
        context.moveToThread(guiThread);
        surface.moveToThread(guiThread);
        QThread::currentThread()->exit(0);
    }

Q_SIGNALS:
    void taskConfigured();
    void taskRendered();

private:
    void initialize()
    {
        context.makeCurrent(&surface);
        context.initializeOpenGLFunctions();

        mDebugLogger.reset(new QOpenGLDebugLogger());
        if (mDebugLogger->initialize()) {
            mDebugLogger->disableMessages(QOpenGLDebugMessage::AnySource,
                QOpenGLDebugMessage::AnyType, QOpenGLDebugMessage::NotificationSeverity);
            connect(mDebugLogger.data(), &QOpenGLDebugLogger::messageLogged,
                this, &Worker::handleDebugMessage);
            mDebugLogger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
        }
    }

    void handleDebugMessage(const QOpenGLDebugMessage &message)
    {
        auto lock = QMutexLocker(&gFirstGLErrorMutex);
        if (message.severity() == QOpenGLDebugMessage::HighSeverity) {
            if (gFirstGLError.isEmpty())
                gFirstGLError = message.message();
        }
        else {
            qDebug() << message.message();
        }
    }

    bool mInitialized{ };
    QScopedPointer<QOpenGLDebugLogger> mDebugLogger;
};

Renderer::Renderer(QObject *parent)
    : QObject(parent)
    , mWorker(new Worker())
{
    mWorker->context.setShareContext(QOpenGLContext::globalShareContext());
    mWorker->context.create();

    mWorker->surface.setFormat(mWorker->context.format());
    mWorker->surface.create();
    
    mWorker->context.moveToThread(&mThread);
    mWorker->surface.moveToThread(&mThread);
    mWorker->moveToThread(&mThread);

    connect(this, &Renderer::configureTask,
        mWorker.data(), &Worker::handleConfigureTask);
    connect(mWorker.data(), &Worker::taskConfigured,
        this, &Renderer::handleTaskConfigured);
    connect(this, &Renderer::renderTask,
        mWorker.data(), &Worker::handleRenderTask);
    connect(mWorker.data(), &Worker::taskRendered,
        this, &Renderer::handleTaskRendered);
    connect(this, &Renderer::releaseTask,
        mWorker.data(), &Worker::handleReleaseTask);

    mThread.start();
}

Renderer::~Renderer()
{
    mPendingTasks.clear();
    
    QMetaObject::invokeMethod(mWorker.data(), "stop", Qt::QueuedConnection);
    mThread.wait();

    mWorker.reset();
}

void Renderer::render(RenderTask *task)
{
    Q_ASSERT(!mPendingTasks.contains(task));
    mPendingTasks.append(task);

    renderNextTask();
}

void Renderer::release(RenderTask *task)
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

void Renderer::renderNextTask()
{
    if (mCurrentTask || mPendingTasks.isEmpty())
        return;

    mCurrentTask = mPendingTasks.takeFirst();
    Q_EMIT configureTask(mCurrentTask, QPrivateSignal());
}

void Renderer::handleTaskConfigured()
{
    mCurrentTask->configured();

    Q_EMIT renderTask(mCurrentTask, QPrivateSignal());
}

void Renderer::handleTaskRendered()
{
    auto currentTask = std::exchange(mCurrentTask, nullptr);
    currentTask->handleRendered();

    renderNextTask();
}

#include "Renderer.moc"
