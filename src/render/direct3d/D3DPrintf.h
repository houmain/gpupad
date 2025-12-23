#pragma once

#if defined(_WIN32)

#include "D3DContext.h"
#include "render/ShaderPrintf.h"

class D3DPrintf : public ShaderPrintf
{
public:
    void beginDownload(D3DContext &context);
    MessagePtrSet finishDownload(ItemId callItemId);
};

#endif // _WIN32
