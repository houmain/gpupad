#pragma once

#include "D3DContext.h"
#include "render/ShaderPrintf.h"

class D3DPrintf : public ShaderPrintf
{
public:
    MessagePtrSet formatMessages(D3DContext &context, ItemId callItemId);
};
