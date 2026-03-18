
#include "GLWindow.h"
#include <QOpenGLContext>

GLWindow::GLWindow()
{
    setSurfaceType(QWindow::OpenGLSurface);
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
    mDebugLogger.reset();
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
        mContext->setShareContext(QOpenGLContext::globalShareContext());
        if (mContext->create()) {
            mContext->makeCurrent(this);
            initializeGL();
        }
    }

    if (initialized()) {
        mContext->makeCurrent(this);

        const qreal dpr = devicePixelRatio();
        glViewport(0, 0, width() * dpr, height() * dpr);

        paintGL();
        mContext->swapBuffers(this);
    }
}

void GLWindow::handleDebugMessage(const QOpenGLDebugMessage &message)
{
    const auto text = message.message();
    qDebug() << text;
}
