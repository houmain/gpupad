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

void initializeCompositorSync() {
    if (!drmAvailable())
        return;

    gFd = open("/dev/dri/card0", O_RDWR);
    if (gFd)
        disableVSync();
}

void synchronizeToCompositor() {
    if (!gFd)
        return;

    auto vbl = drmVBlank{ };
    vbl.request.type = DRM_VBLANK_RELATIVE;
    vbl.request.sequence = 1;
    drmWaitVBlank(gFd, &vbl);
}

#elif defined(_WIN32)

#include <Dwmapi.h>

namespace {
    BOOL gDwmEnabled;
} // namespace

void initializeCompositorSync() {
    DwmIsCompositionEnabled(&gDwmEnabled);
    if (gDwmEnabled)
        disableVSync();
}

void synchronizeToCompositor() {
    if (gDwmEnabled)
        DwmFlush();
}

#endif
