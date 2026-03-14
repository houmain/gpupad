#include "GLRenderer.h"
#include "GLContext.h"
#include "render/RenderTask.h"
#include <QApplication>
#include <QMutex>
#include <QOffscreenSurface>
#include <QOpenGLDebugLogger>
#include <QOpenGLFunctions>
#include <QSemaphore>

namespace {
    QMutex gFirstGLErrorMutex;
    QString gFirstGLError;
} // namespace

QString getFirstGLError()
{
    auto lock = QMutexLocker(&gFirstGLErrorMutex);
    return std::exchange(gFirstGLError, "");
}

class GLRenderer::Worker final : public QObject
{
    Q_OBJECT
public:
    explicit Worker(GLRenderer *renderer) : mRenderer(*renderer) { }

    GLContext context;
    QOffscreenSurface surface;

    void handleConfigureTask(RenderTask *renderTask)
    {
        if (!mInitialized)
            if (initialize())
                mInitialized = true;
        if (mInitialized)
            renderTask->configure();
        Q_EMIT taskConfigured();
    }

    void handleRenderTask(RenderTask *renderTask)
    {
        if (mInitialized)
            renderTask->render();
        Q_EMIT taskRendered();
    }

    void handleReleaseTask(RenderTask *renderTask, void *userData)
    {
        if (mInitialized)
            renderTask->release();
        static_cast<QSemaphore *>(userData)->release(1);
    }

public Q_SLOTS:
    void stop()
    {
        if (mInitialized) {
            mDebugLogger.reset();
            context.doneCurrent();
        }
        auto guiThread = QApplication::instance()->thread();
        context.moveToThread(guiThread);
        surface.moveToThread(guiThread);
        QThread::currentThread()->exit(0);
    }

Q_SIGNALS:
    void taskConfigured();
    void taskRendered();

private:
    bool initialize()
    {
        if (!context.makeCurrent(&surface)
            || !context.initializeOpenGLFunctions()) {
            mMessages +=
                MessageList::insert(0, MessageType::OpenGLVersionNotAvailable, "4.5");
            mRenderer.setFailed();
            return false;
        }

        mDebugLogger = std::make_unique<QOpenGLDebugLogger>();

        if (mDebugLogger->initialize()) {
            mDebugLogger->disableMessages(QOpenGLDebugMessage::AnySource,
                QOpenGLDebugMessage::AnyType,
                QOpenGLDebugMessage::NotificationSeverity
                    | QOpenGLDebugMessage::LowSeverity
                    | QOpenGLDebugMessage::MediumSeverity);

            connect(mDebugLogger.get(), &QOpenGLDebugLogger::messageLogged,
                this, &Worker::handleDebugMessage);
            mDebugLogger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
        }
        return true;
    }

    void handleDebugMessage(const QOpenGLDebugMessage &message)
    {
        const auto lock = QMutexLocker(&gFirstGLErrorMutex);
        Q_ASSERT(message.severity() == QOpenGLDebugMessage::HighSeverity);

        if (gFirstGLError.isEmpty())
            gFirstGLError = message.message();

#if !defined(NDEBUG)
        qDebug() << message.message();
#endif
    }

    GLRenderer &mRenderer;
    bool mInitialized{};
    std::unique_ptr<QOpenGLDebugLogger> mDebugLogger;
    MessagePtrSet mMessages;
};

GLRenderer::GLRenderer(QObject *parent)
    : QObject(parent)
    , Renderer(RenderAPI::OpenGL)
    , mWorker(std::make_unique<Worker>(this))
{
    mWorker->context.setShareContext(QOpenGLContext::globalShareContext());
    mWorker->context.create();

    mWorker->surface.setFormat(mWorker->context.format());
    mWorker->surface.create();

    mWorker->context.moveToThread(&mThread);
    mWorker->surface.moveToThread(&mThread);
    mWorker->moveToThread(&mThread);

    connect(this, &GLRenderer::configureTask, mWorker.get(),
        &Worker::handleConfigureTask);
    connect(mWorker.get(), &Worker::taskConfigured, this,
        &GLRenderer::handleTaskConfigured);
    connect(this, &GLRenderer::renderTask, mWorker.get(),
        &Worker::handleRenderTask);
    connect(mWorker.get(), &Worker::taskRendered, this,
        &GLRenderer::handleTaskRendered);
    connect(this, &GLRenderer::releaseTask, mWorker.get(),
        &Worker::handleReleaseTask);

    mThread.start();
}

GLRenderer::~GLRenderer()
{
    mPendingTasks.clear();

    QMetaObject::invokeMethod(mWorker.get(), "stop", Qt::QueuedConnection);
    mThread.wait();

    mWorker.reset();
}

void GLRenderer::render(RenderTask *task)
{
    Q_ASSERT(!mPendingTasks.contains(task));
    mPendingTasks.append(task);

    renderNextTask();
}

void GLRenderer::release(RenderTask *task)
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

void GLRenderer::renderNextTask()
{
    if (mCurrentTask || mPendingTasks.isEmpty())
        return;

    mCurrentTask = mPendingTasks.takeFirst();
    Q_EMIT configureTask(mCurrentTask, QPrivateSignal());
}

void GLRenderer::handleTaskConfigured()
{
    mCurrentTask->configured();

    Q_EMIT renderTask(mCurrentTask, QPrivateSignal());
}

void GLRenderer::handleTaskRendered()
{
    auto currentTask = std::exchange(mCurrentTask, nullptr);
    currentTask->handleRendered();

    renderNextTask();
}

#include "GLRenderer.moc"
