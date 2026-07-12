#include "GLWindow.h"
#include <QOffscreenSurface>
#include <QPlatformSurfaceEvent>
#include <QScopeGuard>

AdapterIdentity GLWindow::getAdapterIdentity()
{
    Q_ASSERT(onMainThread());
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

//-------------------------------------------------------------------------

GLWindow::GLWindow(bool enableVSync, QWindow *parent)
    : QWindow(parent)
    , mEnableVSync(enableVSync)
{
    setSurfaceType(QWindow::OpenGLSurface);

    auto format = QOpenGLContext::globalShareContext()->format();
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(mEnableVSync ? 1 : 0);
    setFormat(format);
}

GLWindow::~GLWindow()
{
    Q_ASSERT(!initialized());
}

bool GLWindow::event(QEvent *event)
{
    if (event->type() == QEvent::PlatformSurface) {
        auto *surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);

        if (surfaceEvent->surfaceEventType()
            == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            releaseGpu();
        }
    } else if (event->type() == QEvent::UpdateRequest) {
        redraw();
        return true;
    }
    return QWindow::event(event);
}

void GLWindow::exposeEvent(QExposeEvent *event)
{
    QWindow::exposeEvent(event);

    if (isExposed())
        requestUpdate();
}

bool GLWindow::makeCurrent()
{
    if (!mContext) {
        mContext.reset(new QOpenGLContext());
        mContext->setShareContext(QOpenGLContext::globalShareContext());
        if (!mContext->create())
            return false;
    }

    if (!mContext->makeCurrent(this))
        return false;

    if (!mGL) {
        mGL = std::make_unique<GLContext>();
        if (mGL->initialize(mContext.get())) {
            Q_EMIT initializingGpu();
            mInitialized = true;
        }
    }
    return mInitialized;
}

void GLWindow::redraw()
{
    if (!isExposed())
        return;

    if (!makeCurrent())
        return;

    const auto vaoBinder = mGL->bindVertexArrayObject();
    const auto dpr = devicePixelRatio();
    mGL->glViewport(0, 0, width() * dpr, height() * dpr);

    Q_EMIT paintingGpu();

    const auto start = std::chrono::high_resolution_clock::now();
    mContext->swapBuffers(this);
    const auto end = std::chrono::high_resolution_clock::now();

    // limit refresh rate when swapping is not synchronizing
    static auto sLastSwapTime = end;
    if (mEnableVSync && end - start < std::chrono::milliseconds(2)) {
        std::this_thread::sleep_for(sLastSwapTime - end +
            std::chrono::microseconds(16'667));
        sLastSwapTime = end;
    }
    mContext->doneCurrent();
}

void GLWindow::releaseGpu()
{
    if (!mContext || !mInitialized || mReleasing)
        return;

    mReleasing = true;
    if (mContext->makeCurrent(this)) {
        Q_EMIT releasingGpu();
        mGL.reset();
        mContext->doneCurrent();
    }
    mContext.reset();
    mInitialized = false;
    mReleasing = false;
}
