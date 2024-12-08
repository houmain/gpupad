#pragma once

#include "VKContext.h"
#include "render/ShareSync.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QMutex>
#include <QSet>

class VKShareSync : public ShareSync
{
private:
    QMutex mMutex;
    KDGpu::GpuSemaphore mUpdateSemaphore;
    KDGpu::GpuSemaphore mUsageSemaphore;

    void importSemaphore(const KDGpu::GpuSemaphore &gpuSemaphore,
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

        static auto glIsSemaphoreEXT =
            reinterpret_cast<PFNGLISSEMAPHOREEXTPROC>(
                context.getProcAddress("glIsSemaphoreEXT"));
        Q_ASSERT(glIsSemaphoreEXT(*glSemaphore));
    }

public:
    explicit VKShareSync(KDGpu::Device &device)
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

    KDGpu::GpuSemaphore &updateSemaphore() { return mUpdateSemaphore; }
    KDGpu::GpuSemaphore &usageSemaphore() { return mUsageSemaphore; }

    void cleanup()
    {
        QMutexLocker lock{ &mMutex };
        mUpdateSemaphore = {};
        mUsageSemaphore = {};
    }

    void beginUpdate() { mMutex.lock(); }

    void endUpdate() { mMutex.unlock(); }

    void beginUsage(QOpenGLFunctions_3_3_Core &gl) override
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

    void endUsage(QOpenGLFunctions_3_3_Core &gl) override
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
};
