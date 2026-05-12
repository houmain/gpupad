#pragma once

#include "D3DShareSync.h"

D3DShareSync::D3DShareSync(ID3D12Device &device)
{
}

void D3DShareSync::cleanup()
{
    QMutexLocker lock{ &mMutex };
}

void D3DShareSync::beginUpdate()
{
    mMutex.lock();
}

void D3DShareSync::endUpdate()
{
    mMutex.unlock();
}

void D3DShareSync::beginUsage()
{
    mMutex.lock();
}

void D3DShareSync::endUsage()
{
    mMutex.unlock();
}
