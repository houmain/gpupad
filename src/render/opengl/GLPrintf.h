#pragma once

#include "GLItem.h"
#include "render/ShaderPrintf.h"

class GLPrintf : public ShaderPrintf
{
public:
    const GLObject &bufferObject() const { return mBufferObject; }
    void clear();
    MessagePtrSet formatMessages(GLContext &context, ItemId callItemId);

private:
    GLObject mBufferObject;
};
