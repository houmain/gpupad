
#include "GLWidget.h"
#include <QOpenGLContext>

GLWidget::GLWidget(QWidget *parent) : QOpenGLWidget(parent) { }

GLWidget::~GLWidget()
{
    releaseGL();
}

void GLWidget::initializeGL()
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
            this, &GLWidget::handleDebugMessage);
        mDebugLogger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
    }
#endif
    Q_EMIT initializingGL();
}

void GLWidget::paintGL()
{
    if (!mInitialized)
        return;

    QOpenGLVertexArrayObject::Binder vaoBinder(&mVao);
    Q_EMIT paintingGL();
}

void GLWidget::releaseGL()
{
    if (!mInitialized)
        return;

    makeCurrent();
    Q_EMIT releasingGL();
    mDebugLogger.reset();
    doneCurrent();
}

QOpenGLFunctions_4_5_Core &GLWidget::gl()
{
    Q_ASSERT(mInitialized);
    return mGL;
}

void GLWidget::handleDebugMessage(const QOpenGLDebugMessage &message)
{
    const auto text = message.message();
    qDebug() << text;
}
