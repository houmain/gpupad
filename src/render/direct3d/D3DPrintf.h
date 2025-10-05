#pragma once

#if defined(_WIN32)

#include "D3DContext.h"
#include "render/ShaderPrintf.h"

class D3DPrintf : public ShaderPrintf
{
public:
    MessagePtrSet formatMessages(D3DContext &context, ItemId callItemId);
};

#endif // _WIN32
