#include "Renderer.h"
#include <atomic>
#include <QApplication>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOffscreenSurface>
#include <QOpenGLDebugLogger>

class BackgroundRenderer : public QObject
{
    Q_OBJECT

public:
      QOpenGLContext context;
      QOffscreenSurface surface;

public slots:
    void stop() 
    {
        auto guiThread = QApplication::instance()->thread();
        context.moveToThread(guiThread);
        surface.moveToThread(guiThread);
        QThread::currentThread()->exit(0);
    }

    void handleRenderTask(RenderTask* renderTask)
    {
        context.makeCurrent(&surface);
        initializeLogger();
        renderTask->render(context);
        context.doneCurrent();
        emit taskRendered();
    }

    void handleReleaseTask(RenderTask* renderTask, void* userData)
    {
        context.makeCurrent(&surface);
        renderTask->release(context);
        context.doneCurrent();
        static_cast<std::atomic<bool>*>(userData)->store(true);
    }

signals:
    void taskRendered();

private:
    void initializeLogger()
    {        
#if !defined(NDEBUG)
        mDebugLogger.reset(new QOpenGLDebugLogger());
        if (mDebugLogger->initialize()) {
            connect(mDebugLogger.data(), &QOpenGLDebugLogger::messageLogged,
            [this](const QOpenGLDebugMessage& message) {
                qDebug() << message.message();
            });
            mDebugLogger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
        }
#endif
    }

    QScopedPointer<QOpenGLDebugLogger> mDebugLogger;
};

Renderer::Renderer(QObject *parent)
    : QObject(parent)
    , mBackgroundRenderer(new BackgroundRenderer())
{
    auto format = QSurfaceFormat();
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setMajorVersion(4);
    format.setMinorVersion(5);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setOption(QSurfaceFormat::DebugContext);

    mBackgroundRenderer->context.setFormat(format);
    mBackgroundRenderer->context.create();

    mBackgroundRenderer->surface.setFormat(mBackgroundRenderer->context.format());
    mBackgroundRenderer->surface.create();
    
    mBackgroundRenderer->context.moveToThread(&mThread);
    mBackgroundRenderer->surface.moveToThread(&mThread);
    mBackgroundRenderer->moveToThread(&mThread);

    connect(this, &Renderer::renderTask,
        mBackgroundRenderer.data(), &BackgroundRenderer::handleRenderTask);
    connect(mBackgroundRenderer.data(), &BackgroundRenderer::taskRendered,
        this, &Renderer::handleTaskRendered);
    connect(this, &Renderer::releaseTask,
        mBackgroundRenderer.data(), &BackgroundRenderer::handleReleaseTask);

    mThread.start();
}

Renderer::~Renderer()
{
    mPendingTasks.clear();
    
    QMetaObject::invokeMethod(mBackgroundRenderer.data(), "stop", Qt::QueuedConnection);
    mThread.wait();

    mBackgroundRenderer.reset();
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

    std::atomic<bool> released{ false };
    emit releaseTask(task, &released, QPrivateSignal());

    // block until the render thread finished releasing the task
    while (!released)
        QThread::msleep(1);
}

void Renderer::renderNextTask()
{
    if (mCurrentTask || mPendingTasks.isEmpty())
        return;

    mCurrentTask = mPendingTasks.takeFirst();
    mCurrentTask->prepare();

    emit renderTask(mCurrentTask, QPrivateSignal());
}

void Renderer::handleTaskRendered()
{
    mCurrentTask->finish();
    mCurrentTask->validate();
    mCurrentTask = nullptr;

    renderNextTask();
}

#include "Renderer.moc"
