#include "CompositorSync.h"

#include <QSurfaceFormat>

namespace {
    void disableVSync() {
        auto format = QSurfaceFormat::defaultFormat();
        format.setSwapInterval(0);
        QSurfaceFormat::setDefaultFormat(format);
    }
} // namespace

#if defined(__linux)

#include <fcntl.h>
#include <xf86drm.h>

namespace {
    int gFd;
} // namespace

bool initializeCompositorSync() {
    if (!drmAvailable())
        return false;

    gFd = open("/dev/dri/card0", O_RDWR);
    if (!gFd)
        return false;

    disableVSync();
    return true;
}

bool synchronizeToCompositor() {
    if (!gFd)
        return false;

    auto vbl = drmVBlank{ };
    vbl.request.type = DRM_VBLANK_RELATIVE;
    vbl.request.sequence = 1;
    if (drmWaitVBlank(gFd, &vbl) < 0)
        return false;

    return true;
}

#elif defined(_WIN32)

#include <dwmapi.h>

namespace {
    BOOL gDwmEnabled;
} // namespace

bool initializeCompositorSync() {
    if (!SUCCEEDED(DwmIsCompositionEnabled(&gDwmEnabled)) || !gDwmEnabled)
        return false;

    disableVSync();
    return true;
}

bool synchronizeToCompositor() {
    return (gDwmEnabled && SUCCEEDED(DwmFlush()));
}

#endif
