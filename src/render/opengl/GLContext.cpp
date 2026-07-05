#include "GLContext.h"
#include <QDebug>
#include <utility>

GLContext::GLContext(QObject *parent) : QObject(parent) { }

GLContext::~GLContext()
{
    Q_ASSERT(!mContext || QOpenGLContext::currentContext() == mContext);
}

bool GLContext::initialize(QOpenGLContext *context)
{
    Q_ASSERT(!mDebugLogger);
    if (!initializeOpenGLFunctions())
        return false;

    mDebugLogger = std::make_unique<QOpenGLDebugLogger>();
    mDebugLogger->initialize();

    mDebugLogger->disableMessages(QOpenGLDebugMessage::AnySource,
        QOpenGLDebugMessage::AnyType,
        QOpenGLDebugMessage::NotificationSeverity
            | QOpenGLDebugMessage::LowSeverity
            | QOpenGLDebugMessage::MediumSeverity);

    QObject::connect(mDebugLogger.get(), &QOpenGLDebugLogger::messageLogged,
        this, &GLContext::handleDebugMessage);
    mDebugLogger->startLogging(QOpenGLDebugLogger::SynchronousLogging);

    mVertexArrayObject = std::make_unique<QOpenGLVertexArrayObject>();
    if (!mVertexArrayObject->create())
        return false;

    mContext = context;
    return true;
}

QOpenGLVertexArrayObject::Binder GLContext::bindVertexArrayObject()
{
    return QOpenGLVertexArrayObject::Binder(mVertexArrayObject.get());
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
