#pragma once

#include "VKBuffer.h"
#include "render/ShaderPrintf.h"

class VKPrintf : public ShaderPrintf
{
public:
    VKBuffer &getInitializedBuffer(VKContext &context);
    void beginDownload(VKContext &context);
    MessagePtrSet finishDownload(ItemId callItemId);

private:
    std::optional<VKBuffer> mBuffer;
};
