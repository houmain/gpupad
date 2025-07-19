
#include "AdapterIdentity.h"
#include <QOpenGLContext>
#include <QOffscreenSurface>

AdapterIdentity getOpenGLAdapterIdentity()
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

    if (!glContext.hasExtension("GL_EXT_memory_object"))
        return {};
    const auto glGetUnsignedBytevEXT =
        reinterpret_cast<PFNGLGETUNSIGNEDBYTEVEXTPROC>(
            glContext.getProcAddress("glGetUnsignedBytevEXT"));
    if (!glGetUnsignedBytevEXT)
        return {};

    const auto glGetUnsignedBytei_vEXT =
        reinterpret_cast<PFNGLGETUNSIGNEDBYTEI_VEXTPROC>(
            glContext.getProcAddress("glGetUnsignedBytei_vEXT"));
    if (!glGetUnsignedBytei_vEXT)
        return {};

    auto identity = AdapterIdentity{};
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
