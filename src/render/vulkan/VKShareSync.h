#pragma once

#include "VKContext.h"
#include "render/ShareSync.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QMutex>

class VKShareSync : public ShareSync
{
public:
    explicit VKShareSync(KDGpu::Device &device);
    KDGpu::GpuSemaphore &updateSemaphore();
    KDGpu::GpuSemaphore &usageSemaphore();
    void cleanup();
    void beginUpdate();
    void endUpdate();
    void beginUsage(QOpenGLFunctions_4_5_Core &gl) override;
    void endUsage(QOpenGLFunctions_4_5_Core &gl) override;

private:
    QMutex mMutex;
    KDGpu::GpuSemaphore mUpdateSemaphore;
    KDGpu::GpuSemaphore mUsageSemaphore;

    void importSemaphore(const KDGpu::GpuSemaphore &gpuSemaphore,
        GLuint *glSemaphore);
};
