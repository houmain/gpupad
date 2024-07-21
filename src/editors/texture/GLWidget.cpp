
#include "GLWidget.h"
#include <QOpenGLContext>

GLWidget::GLWidget(QWidget *parent) : QOpenGLWidget(parent) { }

GLWidget::~GLWidget()
{
    if (mInitialized) {
        if (context()->makeCurrent(nullptr)) {
            releaseGL();
            context()->doneCurrent();
        }
    }
}

void GLWidget::initializeGL()
{
    mInitialized = true;
    mGL.initializeOpenGLFunctions();

    mGL42.emplace();
    if (!mGL42->initializeOpenGLFunctions())
        mGL42.reset();

    mGL45.emplace();
    if (!mGL45->initializeOpenGLFunctions())
        mGL45.reset();

#if !defined(NDEBUG)
    if (mDebugLogger.initialize()) {
        mDebugLogger.disableMessages(QOpenGLDebugMessage::AnySource,
            QOpenGLDebugMessage::AnyType,
            QOpenGLDebugMessage::NotificationSeverity);
        QObject::connect(&mDebugLogger, &QOpenGLDebugLogger::messageLogged,
            this, &GLWidget::handleDebugMessage);
        mDebugLogger.startLogging(QOpenGLDebugLogger::SynchronousLogging);
    }
#endif
    Q_EMIT initializingGL();
}

void GLWidget::paintGL()
{
    QOpenGLVertexArrayObject::Binder vaoBinder(&mVao);
    Q_EMIT paintingGL();
}

void GLWidget::releaseGL()
{
    Q_EMIT releasingGL();
}

QOpenGLFunctions_3_3_Core &GLWidget::gl()
{
    return mGL;
}

QOpenGLFunctions_4_2_Core *GLWidget::gl42()
{
    return (mGL42.has_value() ? &mGL42.value() : nullptr);
}

QOpenGLFunctions_4_5_Core *GLWidget::gl45()
{
    return (mGL45.has_value() ? &mGL45.value() : nullptr);
}

void GLWidget::handleDebugMessage(const QOpenGLDebugMessage &message)
{
    const auto text = message.message();
    qDebug() << text;
}
