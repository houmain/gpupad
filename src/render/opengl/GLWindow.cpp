#include "GLWindow.h"

#include <QOffscreenSurface>
#include <QScopeGuard>
#include <algorithm>
#include <thread>

AdapterIdentity GLWindow::getAdapterIdentity()
{
    auto glContext = QOpenGLContext();
    glContext.setShareContext(QOpenGLContext::globalShareContext());
    auto surface = QOffscreenSurface();
    surface.setFormat(glContext.format());
    surface.create();
    glContext.create();
    if (!glContext.makeCurrent(&surface))
        return {};
    const auto guard = QScopeGuard([&]() { glContext.doneCurrent(); });

    auto identity = AdapterIdentity{};
    if (auto renderer =
            reinterpret_cast<const char *>(glGetString(GL_RENDERER))) {
        identity.name = QString::fromUtf8(renderer);
        if (auto pos = identity.name.indexOf('/'); pos > 0)
            identity.name.resize(pos);
    }

    if (!glContext.hasExtension("GL_EXT_memory_object"))
        return identity;

    const auto glGetUnsignedBytevEXT =
        reinterpret_cast<PFNGLGETUNSIGNEDBYTEVEXTPROC>(
            glContext.getProcAddress("glGetUnsignedBytevEXT"));
    if (!glGetUnsignedBytevEXT)
        return identity;

    const auto glGetUnsignedBytei_vEXT =
        reinterpret_cast<PFNGLGETUNSIGNEDBYTEI_VEXTPROC>(
            glContext.getProcAddress("glGetUnsignedBytei_vEXT"));
    if (!glGetUnsignedBytei_vEXT)
        return identity;

    static_assert(GL_UUID_SIZE_EXT == sizeof(AdapterIdentity::UUID));
    auto numDeviceUuids = GLint{};
    glGetIntegerv(GL_NUM_DEVICE_UUIDS_EXT, &numDeviceUuids);
    for (auto i = 0; i < std::min(numDeviceUuids, 4); ++i)
        glGetUnsignedBytei_vEXT(GL_DEVICE_UUID_EXT, i,
            identity.deviceUUIDs[i].data());
    glGetUnsignedBytevEXT(GL_DRIVER_UUID_EXT, identity.driverUUID.data());

#if defined(_WIN32)
    glGetUnsignedBytevEXT(GL_DEVICE_LUID_EXT, identity.deviceLUID.data());
#endif
    return identity;
}

GLWindow::GLWindow(int syncInterval)
    : mSyncInterval(syncInterval)
    , mContext(std::make_unique<GLContext>(this))
{
    setSurfaceType(QWindow::OpenGLSurface);

    auto format = QSurfaceFormat::defaultFormat();
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(syncInterval);
    setFormat(format);
}

GLWindow::~GLWindow()
{
    releaseGL();
}

void GLWindow::initializeGL()
{
    auto &gl = context();
    if (QOpenGLContext::currentContext() != &gl)
        return;
    if (!gl.initializeCurrentContext())
        return;

    mInitialized = true;

    Q_EMIT initializingGpu();
}

void GLWindow::releaseGL()
{
    if (!mInitialized)
        return;

    if (mContext && mContext->makeCurrent(this)) {
        Q_EMIT releasingGpu();
        mContext->shutdownCurrentContext();
        mContext->doneCurrent();
    }
    mInitialized = false;
}

void GLWindow::paintGL()
{
    if (!mInitialized)
        return;

    auto vaoBinder = context().bindVertexArrayObject();
    Q_EMIT paintingGpu();
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

    if (!makeCurrent())
        return;

    const qreal dpr = devicePixelRatio();
    auto &gl = context();
    gl.glViewport(0, 0, width() * dpr, height() * dpr);
    paintGL();

    const auto start = std::chrono::high_resolution_clock::now();
    gl.swapBuffers(this);
    const auto end = std::chrono::high_resolution_clock::now();

    // limit refresh rate when swapping is not synchronizing
    if (mSyncInterval && end - start < std::chrono::milliseconds(2)) {
        std::this_thread::sleep_for(mLastSwapTime - end
            + std::chrono::microseconds(16'667 * mSyncInterval));
        mLastSwapTime = end;
    }
}

bool GLWindow::makeCurrent()
{
    auto &gl = context();
    if (!gl.isValid()) {
        gl.setFormat(format());
        if (auto *shareContext = QOpenGLContext::globalShareContext())
            gl.setShareContext(shareContext);
        if (!gl.create())
            return false;
    }

    if (!gl.makeCurrent(this))
        return false;

    if (!initialized())
        initializeGL();

    return initialized();
}
