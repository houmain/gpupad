#include "GLContext.h"

#include <QDebug>
#include <utility>

bool GLContext::initializeCurrentContext(bool suppressLowSeverityMessages)
{
    if (!initializeOpenGLFunctions())
        return false;

    if (!mVertexArrayObject.isCreated())
        mVertexArrayObject.create();

    if (mDebugLogger)
        return true;

    mDebugLogger = std::make_unique<QOpenGLDebugLogger>();
    if (!mDebugLogger->initialize()) {
        mDebugLogger.reset();
        return true;
    }

    auto disabledSeverities =
        QOpenGLDebugMessage::Severities{ QOpenGLDebugMessage::NotificationSeverity };
    if (suppressLowSeverityMessages) {
        disabledSeverities |= QOpenGLDebugMessage::LowSeverity
            | QOpenGLDebugMessage::MediumSeverity;
    }

    mDebugLogger->disableMessages(QOpenGLDebugMessage::AnySource,
        QOpenGLDebugMessage::AnyType, disabledSeverities);

    QObject::connect(mDebugLogger.get(), &QOpenGLDebugLogger::messageLogged,
        this, &GLContext::handleDebugMessage);
    mDebugLogger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
    return true;
}

void GLContext::shutdownCurrentContext()
{
    mVertexArrayObject.destroy();
    mDebugLogger.reset();
    mLastGLError.clear();
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
