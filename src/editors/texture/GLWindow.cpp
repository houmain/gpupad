
#include "GLWindow.h"
#include <QOpenGLContext>

GLWindow::GLWindow() : GLWindowBase()
{
#if defined(USE_WINDOW_CONTAINER)
    setSurfaceType(QWindow::OpenGLSurface);
#endif
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

#if defined(USE_WINDOW_CONTAINER)
    m_context->makeCurrent(this);
#else
    makeCurrent();
#endif
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

#if defined(USE_WINDOW_CONTAINER)

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

    if (!m_context) {
        m_context = new QOpenGLContext(this);
        m_context->setShareContext(QOpenGLContext::globalShareContext());
        m_context->setFormat(QSurfaceFormat::defaultFormat());
        if (m_context->create()) {
            m_context->makeCurrent(this);
            initializeGL();
        }
    }

    if (initialized()) {
        m_context->makeCurrent(this);

        const qreal dpr = devicePixelRatio();
        glViewport(0, 0, width() * dpr, height() * dpr);

        paintGL();
        m_context->swapBuffers(this);
    }
}

#endif // defined(USE_WINDOW_CONTAINER)

void GLWindow::handleDebugMessage(const QOpenGLDebugMessage &message)
{
    const auto text = message.message();
    qDebug() << text;
}
