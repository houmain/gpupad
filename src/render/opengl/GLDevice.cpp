#include "GLDevice.h"

#if defined(OPENGL_ENABLED)

#  include <QApplication>

GLDevice::GLDevice()
    : Device(Type::OpenGL)
    , mContext(this)
    , mSurface(new QOffscreenSurface(nullptr, qApp))
    , mGL(this)
{
    Q_ASSERT(QThread::currentThread() == qApp->thread());
    mSurface->setFormat(mContext.format());
    mSurface->create();
    connect(this, &QObject::destroyed, mSurface, &QObject::deleteLater);
}

GLDevice::~GLDevice()
{
    Q_ASSERT(!mContext.isValid()
        || QOpenGLContext::currentContext() == &mContext);
}

bool GLDevice::initialize()
{
    Q_ASSERT(mContext.thread() == QThread::currentThread());

    mContext.setShareContext(QOpenGLContext::globalShareContext());
    if (!mContext.create() || !mContext.makeCurrent(mSurface)
        || !mGL.initialize(&mContext)) {
        mMessages.insert(MessageType::OpenGLVersionNotAvailable, "4.5");
        return false;
    }
    return true;
}

#endif // defined(OPENGL_ENABLED)
