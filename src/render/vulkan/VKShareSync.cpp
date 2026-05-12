
#include "VKShareSync.h"

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

void VKShareSync::beginUsage()
{
    mMutex.lock();
}

void VKShareSync::endUsage()
{
    mMutex.unlock();
}
