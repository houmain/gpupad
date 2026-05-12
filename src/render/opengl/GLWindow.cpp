#include "GLWindow.h"

GLWindow::GLWindow(int syncInterval) : mSyncInterval(syncInterval)
{
    setSurfaceType(QWindow::OpenGLSurface);

    auto format = QSurfaceFormat::defaultFormat();
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(syncInterval);
    setFormat(format);
}

GLWindow::~GLWindow()
{
    releaseGL();
}

void GLWindow::initializeGL()
{
    if (!mContext->initializeOpenGLFunctions())
        return;

    if (!mVao.isCreated())
        mVao.create();

    mInitialized = true;

#if !defined(NDEBUG)
    mDebugLogger = std::make_unique<QOpenGLDebugLogger>();
    if (mDebugLogger->initialize()) {
        mDebugLogger->disableMessages(QOpenGLDebugMessage::AnySource,
            QOpenGLDebugMessage::AnyType,
            QOpenGLDebugMessage::NotificationSeverity);
        QObject::connect(mDebugLogger.get(), &QOpenGLDebugLogger::messageLogged,
            this, &GLWindow::handleDebugMessage);
        mDebugLogger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
    }
#endif
    Q_EMIT initializingGpu();
}

void GLWindow::releaseGL()
{
    if (!mInitialized)
        return;

    mContext->makeCurrent(this);
    Q_EMIT releasingGpu();
#if !defined(NDEBUG)
    mDebugLogger.reset();
#endif
    mVao.destroy();
    mContext->doneCurrent();
    mInitialized = false;
}

void GLWindow::paintGL()
{
    if (!mInitialized)
        return;

    QOpenGLVertexArrayObject::Binder vaoBinder(&mVao);
    Q_EMIT paintingGpu();
}

bool GLWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::UpdateRequest: update(); return true;
    default:                    return QWindow::event(event);
    }
}

void GLWindow::exposeEvent(QExposeEvent *)
{
    if (isExposed())
        update();
}

void GLWindow::update()
{
    if (!isExposed())
        return;

    if (!makeCurrent())
        return;

    const qreal dpr = devicePixelRatio();
    mContext->glViewport(0, 0, width() * dpr, height() * dpr);
    paintGL();

    const auto start = std::chrono::high_resolution_clock::now();
    mContext->swapBuffers(this);
    const auto end = std::chrono::high_resolution_clock::now();

    // limit refresh rate when swapping is not synchronizing
    if (mSyncInterval && end - start < std::chrono::milliseconds(2)) {
        std::this_thread::sleep_for(mLastSwapTime - end
            + std::chrono::microseconds(16'667 * mSyncInterval));
        mLastSwapTime = end;
    }
}

bool GLWindow::makeCurrent()
{
    if (!mContext) {
        mContext = new GLContext(this);
        mContext->setFormat(format());
        if (auto *shareContext = QOpenGLContext::globalShareContext())
            mContext->setShareContext(shareContext);

        if (mContext->create()) {
            mContext->makeCurrent(this);
            initializeGL();
        }
    }

    if (!initialized())
        return false;

    return mContext->makeCurrent(this);
}

void GLWindow::handleDebugMessage(const QOpenGLDebugMessage &message)
{
    const auto text = message.message();
    qDebug() << text;
}
