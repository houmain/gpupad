#include "GLDevice.h"

#if defined(OPENGL_ENABLED)

#  include <QApplication>

GLDevice::GLDevice(QObject *parent)
    : Device(Type::OpenGL)
{
    createContext(parent);
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

    if (!mContext || !mSurface)
        createRendererContext();
    if (!mContext->makeCurrent(mSurface.get()) ||
        !mContext->initialize()) {
        mMessages.insert(0, MessageType::OpenGLVersionNotAvailable, "4.5");
        return false;
    }
    mInitialized = true;
    return true;
}

void GLDevice::shutdown()
{
    if (mContext
        && (mInitialized || QOpenGLContext::currentContext() == mContext.get())) {
        auto current = QOpenGLContext::currentContext() == mContext.get();
        if (!current && mSurface)
            current = mContext->makeCurrent(mSurface.get());
        if (current) {
            mContext->release();
            mContext->doneCurrent();
        }
    }
    mInitialized = false;

    if (mContext && mSurface) {
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

#endif // defined(OPENGL_ENABLED)
