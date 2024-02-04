#pragma once

#include "GLItem.h"
#include "../ShaderPrintf.h"

class GLPrintf : public ShaderPrintf
{
public:
    const GLObject &bufferObject() const { return mBufferObject; }
    void clear();
    MessagePtrSet formatMessages(ItemId callItemId);

private:
    GLObject mBufferObject;
};
