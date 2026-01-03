#pragma once

#if defined(_WIN32)

#include "D3DBuffer.h"
#include "render/ShaderPrintf.h"

class D3DPrintf : public ShaderPrintf
{
public:
    D3DBuffer &getInitializedBuffer(D3DContext &context);
    void beginDownload(D3DContext &context);
    MessagePtrSet finishDownload(ItemId callItemId);

private:
    std::optional<D3DBuffer> mBuffer;
};

#endif // _WIN32
