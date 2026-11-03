#pragma once

#include "../AccelerationStructureBase.h"
#include "GLContext.h"

class GLBuffer;

class GLGeometrySet
{
public:
    explicit GLGeometrySet(const Item &) { }
    bool operator==(const GLGeometrySet &) const { return true; }
    void setVertexBuffer(int, GLBuffer *, const Block &, GLRenderSession &) { }
    void setIndexBuffer(int, GLBuffer *, const Block &, GLRenderSession &) { }
    void setTransformBuffer(int, GLBuffer *, const Block &, GLRenderSession &)
    {
    }
};

class GLAccelerationStructure : public AccelerationStructureBase
{
public:
    explicit GLAccelerationStructure(const AccelerationStructure &accelStruct)
    {
    }

    bool operator==(const GLAccelerationStructure &rhs) const { return true; }

    void setGeometrySet(int, GLGeometrySet *) { }
    template <typename GeometrySets>
    void relinkGeometrySets(GeometrySets &)
    {
    }

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
