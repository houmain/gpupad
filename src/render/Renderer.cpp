#include "Renderer.h"
#include "RenderTask.h"
#include "GLContext.h"
#include <QApplication>
#include <QOpenGLFunctions>
#include <QOffscreenSurface>
#include <QOpenGLDebugLogger>
#include <QSemaphore>

class Renderer::Worker : public QObject
{
    Q_OBJECT

public:
      GLContext context;
      QOffscreenSurface surface;

public slots:
    void stop() 
    {
        context.makeCurrent(&surface);
        mDebugLogger.reset();
        context.doneCurrent();

        auto guiThread = QApplication::instance()->thread();
        context.moveToThread(guiThread);
        surface.moveToThread(guiThread);
        QThread::currentThread()->exit(0);
    }

    void handleRenderTask(RenderTask* renderTask)
    {
        context.makeCurrent(&surface);

        if (!std::exchange(mInitialized, true))
            initialize();

        renderTask->render();
        context.doneCurrent();
        emit taskRendered();
    }

    void handleReleaseTask(RenderTask* renderTask, void* userData)
    {
        context.makeCurrent(&surface);
        renderTask->release();
        context.doneCurrent();
        static_cast<QSemaphore*>(userData)->release(1);
    }

signals:
    void taskRendered();

private:
    void initialize()
    {
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
        auto text = message.message();
        qDebug() << text;
    }

    bool mInitialized{ };
    QScopedPointer<QOpenGLDebugLogger> mDebugLogger;
};

Renderer::Renderer(QObject *parent)
    : QObject(parent)
    , mWorker(new Worker())
{
    auto format = QSurfaceFormat();
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setMajorVersion(4);
    format.setMinorVersion(5);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setOption(QSurfaceFormat::DebugContext);

    mWorker->context.setFormat(format);
    mWorker->context.create();

    mWorker->surface.setFormat(mWorker->context.format());
    mWorker->surface.create();
    
    mWorker->context.moveToThread(&mThread);
    mWorker->surface.moveToThread(&mThread);
    mWorker->moveToThread(&mThread);

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
    emit releaseTask(task, &done, QPrivateSignal());

    // block until the render thread finished releasing the task
    done.acquire(1);
    done.release(1);
}

void Renderer::renderNextTask()
{
    if (mCurrentTask || mPendingTasks.isEmpty())
        return;

    mCurrentTask = mPendingTasks.takeFirst();
    emit renderTask(mCurrentTask, QPrivateSignal());
}

void Renderer::handleTaskRendered()
{
    mCurrentTask->handleRendered();
    mCurrentTask = nullptr;

    renderNextTask();
}

#include "Renderer.moc"
