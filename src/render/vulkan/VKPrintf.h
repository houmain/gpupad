#pragma once

#include "VKContext.h"
#include "render/ShaderPrintf.h"

class VKPrintf : public ShaderPrintf
{
public:
    KDGpu::Buffer &getInitializedBuffer(VKContext &context);
    void beginDownload(VKContext &context);
    MessagePtrSet finishDownload(ItemId callItemId);

private:
    KDGpu::Buffer mBuffer;
    KDGpu::Buffer mDownloadBuffer;
};
