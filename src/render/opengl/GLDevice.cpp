#include "GLDevice.h"

#if defined(OPENGL_ENABLED)

#  include <QApplication>

GLDevice::GLDevice(QObject *parent)
    : Device(Type::OpenGL)
    , mContext(this)
    , mSurface(nullptr, this)
    , mGL(this)
{
}

GLDevice::~GLDevice()
{
    Q_ASSERT(!mContext.isValid()
        || QOpenGLContext::currentContext() == &mContext);
}

bool GLDevice::initialize(const AdapterIdentity &adapterIdentity)
{
    Q_UNUSED(adapterIdentity);
    Q_ASSERT(mContext.thread() == QThread::currentThread());

    mContext.setShareContext(QOpenGLContext::globalShareContext());
    mSurface.setFormat(mContext.format());
    mSurface.create();
    if (!mContext.create() || !mContext.makeCurrent(&mSurface)
        || !mGL.initialize(&mContext)) {
        mMessages.insert(0, MessageType::OpenGLVersionNotAvailable, "4.5");
        return false;
    }
    return true;
}

#endif // defined(OPENGL_ENABLED)
