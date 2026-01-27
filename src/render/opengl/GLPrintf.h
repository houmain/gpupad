#pragma once

#include "GLBuffer.h"
#include "render/PrintfBase.h"

class GLPrintf : public PrintfBase
{
public:
    GLBuffer &getInitializedBuffer(GLContext &context);
    void beginDownload(GLContext &context);
    MessagePtrSet finishDownload(ItemId callItemId);

private:
    std::optional<GLBuffer> mBuffer;
};
