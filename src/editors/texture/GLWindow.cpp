
#include "GLWindow.h"
#include <QOpenGLContext>
#include <QOpenGLPaintDevice>
#include <QPainter>

GLWindow::GLWindow(QWindow *parent) : QWindow(parent)
{
    setSurfaceType(QWindow::OpenGLSurface);
}

GLWindow::~GLWindow()
{
    if (!mInitialized)
        return;

    m_context->makeCurrent(this);
    Q_EMIT releasingGL();
    mDebugLogger.reset();
}

void GLWindow::initialize()
{
    if (!initializeOpenGLFunctions())
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
        m_context->create();
        m_context->makeCurrent(this);
        initialize();
    }

    if (initialized()) {
        m_context->makeCurrent(this);
        QOpenGLVertexArrayObject::Binder vaoBinder(&mVao);

        const qreal dpr = devicePixelRatio();
        glViewport(0, 0, width() * dpr, height() * dpr);

        Q_EMIT paintingGL();
    }

    m_context->swapBuffers(this);
}

void GLWindow::handleDebugMessage(const QOpenGLDebugMessage &message)
{
    const auto text = message.message();
    qDebug() << text;
}
