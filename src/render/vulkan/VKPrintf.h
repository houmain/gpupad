#pragma once

#include "render/ShaderPrintf.h"
#include "VKItem.h"

class VKPrintf : public ShaderPrintf {
public:
    KDGpu::Buffer &getInitializedBuffer(VKContext &context);
    MessagePtrSet formatMessages(VKContext &context, ItemId callItemId);

private:
    KDGpu::Buffer mBuffer;
};
