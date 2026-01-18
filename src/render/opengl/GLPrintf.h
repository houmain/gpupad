#pragma once

#include "GLContext.h"
#include "render/PrintfBase.h"

class GLPrintf : public PrintfBase
{
public:
    const GLObject &bufferObject() const { return mBufferObject; }
    void clear();
    void beginDownload(GLContext &context);
    MessagePtrSet finishDownload(ItemId callItemId);

private:
    GLObject mBufferObject;
    BufferHeader mHeader{ };
    std::vector<uint32_t> mData;
};
