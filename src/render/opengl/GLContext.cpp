#include "GLContext.h"
#include <QDebug>
#include <utility>

GLContext::~GLContext()
{
    Q_ASSERT(!mDebugLogger);
}

bool GLContext::initialize()
{
    Q_ASSERT(!mDebugLogger);
    if (!initializeOpenGLFunctions())
        return false;

    mDebugLogger = std::make_unique<QOpenGLDebugLogger>();
    Q_ASSERT(mDebugLogger->initialize());

    mDebugLogger->disableMessages(QOpenGLDebugMessage::AnySource,
        QOpenGLDebugMessage::AnyType,
        QOpenGLDebugMessage::NotificationSeverity
            | QOpenGLDebugMessage::LowSeverity
            | QOpenGLDebugMessage::MediumSeverity);

    QObject::connect(mDebugLogger.get(), &QOpenGLDebugLogger::messageLogged,
        this, &GLContext::handleDebugMessage);
    mDebugLogger->startLogging(QOpenGLDebugLogger::SynchronousLogging);

    mVertexArrayObject.create();
    return true;
}

void GLContext::release()
{
    Q_ASSERT(QOpenGLContext::currentContext() == this);
    mVertexArrayObject.destroy();
    mDebugLogger.reset();
}

QOpenGLVertexArrayObject::Binder GLContext::bindVertexArrayObject()
{
    return QOpenGLVertexArrayObject::Binder(&mVertexArrayObject);
}

QString GLContext::getLastGLError()
{
    return std::exchange(mLastGLError, {});
}

void GLContext::handleDebugMessage(const QOpenGLDebugMessage &message)
{
    if (message.severity() == QOpenGLDebugMessage::HighSeverity)
        mLastGLError = message.message();

#if !defined(NDEBUG)
    qDebug() << message.message();
#endif
}
