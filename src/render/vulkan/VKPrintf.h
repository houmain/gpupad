#pragma once

#include "VKBuffer.h"
#include "render/PrintfBase.h"

class VKPrintf : public PrintfBase
{
public:
    VKBuffer &getInitializedBuffer(VKContext &context);
    void beginDownload(VKContext &context);
    MessagePtrSet finishDownload(ItemId callItemId);

private:
    std::optional<VKBuffer> mBuffer;
};
