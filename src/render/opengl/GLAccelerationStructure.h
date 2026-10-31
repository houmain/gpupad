#pragma once

#include "../AccelerationStructureBase.h"
#include "GLContext.h"

class GLBuffer;

class GLAccelerationStructure : public AccelerationStructureBase
{
public:
    explicit GLAccelerationStructure(const AccelerationStructure &accelStruct)
    {
    }

    bool operator==(const GLAccelerationStructure &rhs) const { return true; }

    void setVertexBuffer(int instanceIndex, int geometryIndex, GLBuffer *buffer,
        const Block &block, GLRenderSession &renderSession)
    {
    }

    void setIndexBuffer(int instanceIndex, int geometryIndex, GLBuffer *buffer,
        const Block &block, GLRenderSession &renderSession)
    {
    }

    void setTransformBuffer(int instanceIndex, int geometryIndex,
        GLBuffer *buffer, const Block &block, GLRenderSession &renderSession)
    {
    }
};
