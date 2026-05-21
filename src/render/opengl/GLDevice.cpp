#include "GLDevice.h"

#if defined(OPENGL_ENABLED)

#  include <QApplication>
#  include <QMutex>

namespace {
    QMutex gFirstGLErrorMutex;
    QString gFirstGLError;
} // namespace

QString getFirstGLError()
{
    auto lock = QMutexLocker(&gFirstGLErrorMutex);
    return std::exchange(gFirstGLError, "");
}

GLDevice::GLDevice(Usage usage, QObject *parent)
    : Device(Type::OpenGL)
    , mUsage(usage)
{
    createContext(parent);
    if (mUsage == Usage::Renderer)
        createRendererContext();
}

GLDevice::~GLDevice()
{
    shutdown();
}

void GLDevice::moveToThread(QThread *thread)
{
    if (mContext)
        mContext->moveToThread(thread);
    if (mSurface)
        mSurface->moveToThread(thread);
}

bool GLDevice::initialize(const AdapterIdentity &adapterIdentity)
{
    Q_UNUSED(adapterIdentity);

    if (mUsage == Usage::Renderer) {
        if (!mContext || !mSurface)
            createRendererContext();
        if (!mContext->makeCurrent(mSurface.get())) {
            mMessages.insert(0, MessageType::OpenGLVersionNotAvailable, "4.5");
            return false;
        }
    } else if (QOpenGLContext::currentContext() != mContext.get()) {
        return false;
    }

    if (!initializeCurrentContext()) {
        mMessages.insert(0, MessageType::OpenGLVersionNotAvailable, "4.5");
        return false;
    }

    mInitialized = true;
    return true;
}

void GLDevice::shutdown()
{
    if (mInitialized) {
        mDebugLogger.reset();
        if (mContext)
            mContext->doneCurrent();
        mInitialized = false;
    }

    if (mUsage == Usage::Renderer && mContext && mSurface) {
        auto guiThread = QApplication::instance()->thread();
        mContext->moveToThread(guiThread);
        mSurface->moveToThread(guiThread);
    }
}

GLContext &GLDevice::context()
{
    Q_ASSERT(mContext);
    return *mContext;
}

QOpenGLFunctions_4_5_Core &GLDevice::gl()
{
    return context();
}

void GLDevice::createContext(QObject *parent)
{
    mContext = std::make_unique<GLContext>(parent);
    if (auto *shareContext = QOpenGLContext::globalShareContext())
        mContext->setShareContext(shareContext);
}

void GLDevice::createRendererContext()
{
    if (!mContext)
        createContext();
    if (!mContext->isValid())
        mContext->create();

    mSurface = std::make_unique<QOffscreenSurface>();
    mSurface->setFormat(mContext->format());
    mSurface->create();
}

bool GLDevice::initializeCurrentContext()
{
    if (mInitialized)
        return true;

    if (!mContext || !mContext->initializeOpenGLFunctions())
        return false;

    mDebugLogger = std::make_unique<QOpenGLDebugLogger>();
    if (mDebugLogger->initialize()) {
        auto disabledSeverities = QOpenGLDebugMessage::Severities{
            QOpenGLDebugMessage::NotificationSeverity
        };
        if (mUsage == Usage::Renderer)
            disabledSeverities |= QOpenGLDebugMessage::LowSeverity
                | QOpenGLDebugMessage::MediumSeverity;

        mDebugLogger->disableMessages(QOpenGLDebugMessage::AnySource,
            QOpenGLDebugMessage::AnyType, disabledSeverities);

        QObject::connect(mDebugLogger.get(), &QOpenGLDebugLogger::messageLogged,
            mDebugLogger.get(), [this](const QOpenGLDebugMessage &message) {
                handleDebugMessage(message);
            });
        mDebugLogger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
    }
    return true;
}

void GLDevice::handleDebugMessage(const QOpenGLDebugMessage &message)
{
    const auto lock = QMutexLocker(&gFirstGLErrorMutex);

    if (message.severity() == QOpenGLDebugMessage::HighSeverity
        && gFirstGLError.isEmpty())
        gFirstGLError = message.message();

#  if !defined(NDEBUG)
    qDebug() << message.message();
#  endif
}

#endif // defined(OPENGL_ENABLED)
