#pragma once

#include "VKContext.h"
#include "render/ShareSync.h"
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
    void beginUsage() override;
    void endUsage() override;

private:
    QMutex mMutex;
    KDGpu::GpuSemaphore mUpdateSemaphore;
    KDGpu::GpuSemaphore mUsageSemaphore;

    void importSemaphore(const KDGpu::GpuSemaphore &gpuSemaphore,
        GLuint *glSemaphore);
};
