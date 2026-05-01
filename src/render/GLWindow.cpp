
#include "GLWindow.h"
#include <QOpenGLContext>
#include <thread>

GLWindow::GLWindow() : mSyncInterval(0)
{
    setSurfaceType(QWindow::OpenGLSurface);
}

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
    if (!mGL.initializeOpenGLFunctions())
        return;

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
    Q_EMIT initializingGL();
}

void GLWindow::releaseGL()
{
    if (!mInitialized)
        return;

    mContext->makeCurrent(this);
    Q_EMIT releasingGL();
#if !defined(NDEBUG)
    mDebugLogger.reset();
#endif
}

void GLWindow::paintGL()
{
    if (!mInitialized)
        return;

    QOpenGLVertexArrayObject::Binder vaoBinder(&mVao);
    Q_EMIT paintingGL();
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

    if (!mContext) {
        mContext = new QOpenGLContext(this);

        if (!mSyncInterval)
            mContext->setShareContext(QOpenGLContext::globalShareContext());

        if (mContext->create()) {
            mContext->makeCurrent(this);
            initializeGL();
        }
    }

    if (!initialized())
        return;

    mContext->makeCurrent(this);

    const qreal dpr = devicePixelRatio();
    glViewport(0, 0, width() * dpr, height() * dpr);
    paintGL();

    const auto start = std::chrono::high_resolution_clock::now();
    mContext->swapBuffers(this);
    const auto end = std::chrono::high_resolution_clock::now();

    // limit refresh rate when swapping is not synchronizing
    if (mSyncInterval && end - start < std::chrono::milliseconds(2)) {
        std::this_thread::sleep_for(mLastSwapTime - end + 
            std::chrono::microseconds(16'667 * mSyncInterval));
        mLastSwapTime = end;
    }
}

void GLWindow::handleDebugMessage(const QOpenGLDebugMessage &message)
{
    const auto text = message.message();
    qDebug() << text;
}
