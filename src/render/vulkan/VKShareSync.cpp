#pragma once

#include "VKShareSync.h"

void VKShareSync::importSemaphore(const KDGpu::GpuSemaphore &gpuSemaphore,
    GLuint *glSemaphore)
{
    Q_ASSERT(!*glSemaphore);
    auto &context = *QOpenGLContext::currentContext();

    static auto glGenSemaphoresEXT =
        reinterpret_cast<PFNGLGENSEMAPHORESEXTPROC>(
            context.getProcAddress("glGenSemaphoresEXT"));
    glGenSemaphoresEXT(1, glSemaphore);

    const auto handle = gpuSemaphore.externalSemaphoreHandle();
#if defined(_WIN32)
    static auto glImportSemaphoreWin32HandleEXT =
        reinterpret_cast<PFNGLIMPORTSEMAPHOREWIN32HANDLEEXTPROC>(
            context.getProcAddress("glImportSemaphoreWin32HandleEXT"));
    glImportSemaphoreWin32HandleEXT(*glSemaphore,
        GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, std::get<HANDLE>(handle));
#else
    static auto glImportSemaphoreFdEXT =
        reinterpret_cast<PFNGLIMPORTSEMAPHOREFDEXTPROC>(
            context.getProcAddress("glImportSemaphoreFdEXT"));
    glImportSemaphoreFdEXT(*glSemaphore, GL_HANDLE_TYPE_OPAQUE_FD_EXT,
        std::get<int>(handle));
#endif

    static auto glIsSemaphoreEXT = reinterpret_cast<PFNGLISSEMAPHOREEXTPROC>(
        context.getProcAddress("glIsSemaphoreEXT"));
    Q_ASSERT(glIsSemaphoreEXT(*glSemaphore));
}

VKShareSync::VKShareSync(KDGpu::Device &device)
{
    const auto options = KDGpu::GpuSemaphoreOptions{
        .externalSemaphoreHandleType =
#if defined(_WIN32)
            KDGpu::ExternalSemaphoreHandleTypeFlagBits::OpaqueWin32
#else
            KDGpu::ExternalSemaphoreHandleTypeFlagBits::OpaqueFD
#endif
    };

    mUpdateSemaphore = device.createGpuSemaphore(options);
#if 0 // TODO:
    mUsageSemaphore = device.createGpuSemaphore(options);
#endif
}

KDGpu::GpuSemaphore &VKShareSync::updateSemaphore()
{
    return mUpdateSemaphore;
}

KDGpu::GpuSemaphore &VKShareSync::usageSemaphore()
{
    return mUsageSemaphore;
}

void VKShareSync::cleanup()
{
    QMutexLocker lock{ &mMutex };
    mUpdateSemaphore = {};
    mUsageSemaphore = {};
}

void VKShareSync::beginUpdate()
{
    mMutex.lock();
}

void VKShareSync::endUpdate()
{
    mMutex.unlock();
}

void VKShareSync::beginUsage(QOpenGLFunctions_3_3_Core &gl)
{
    mMutex.lock();

#if 0 // TODO:
    auto &context = *QOpenGLContext::currentContext();
    static auto glWaitSemaphoreEXT =
        reinterpret_cast<PFNGLWAITSEMAPHOREEXTPROC>(
            context.getProcAddress("glWaitSemaphoreEXT"));

    static GLuint glSemaphore;
    if (!glSemaphore)
        importSemaphore(mUpdateSemaphore, &glSemaphore);

    // synchronize with end of update
    glWaitSemaphoreEXT(glSemaphore, 0, nullptr, 0, nullptr, nullptr);
#endif
}

void VKShareSync::endUsage(QOpenGLFunctions_3_3_Core &gl)
{
#if 0 // TODO:
    auto &context = *QOpenGLContext::currentContext();
    static auto glSignalSemaphoreEXT =
        reinterpret_cast<PFNGLSIGNALSEMAPHOREEXTPROC>(
            context.getProcAddress("glSignalSemaphoreEXT"));

    static GLuint glSemaphore;
    if (!glSemaphore)
        importSemaphore(mUsageSemaphore, &glSemaphore);

    // mark end of usage
    glSignalSemaphoreEXT(glSemaphore, 0, nullptr, 0, nullptr, nullptr);
    gl.glFlush();
#endif
    mMutex.unlock();
}
