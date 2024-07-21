#pragma once

#include "VKItem.h"
#include "render/ShaderPrintf.h"

class VKPrintf : public ShaderPrintf
{
public:
    KDGpu::Buffer &getInitializedBuffer(VKContext &context);
    MessagePtrSet formatMessages(VKContext &context, ItemId callItemId);

private:
    KDGpu::Buffer mBuffer;
};
