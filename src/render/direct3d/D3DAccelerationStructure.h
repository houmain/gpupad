#pragma once

#if defined(_WIN32)

#include "D3DBuffer.h"
#include "scripting/ScriptEngine.h"

class D3DAccelerationStructure
{
public:
    explicit D3DAccelerationStructure(const AccelerationStructure &accelStruct);
    bool operator==(const D3DAccelerationStructure &rhs) const;
    void setVertexBuffer(int instanceIndex, int geometryIndex, D3DBuffer *buffer,
        const Block &block, D3DRenderSession &renderSession);
    void setIndexBuffer(int instanceIndex, int geometryIndex, D3DBuffer *buffer,
        const Block &block, D3DRenderSession &renderSession);
    void setTransformBuffer(int instanceIndex, int geometryIndex,
        D3DBuffer *buffer, const Block &block, D3DRenderSession &renderSession);

    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    struct D3DGeometry
    {
        bool operator==(const D3DGeometry &) const = default;

        ItemId itemId{};
        Geometry::GeometryType type{};
        D3DBuffer *vertexBuffer{};
        size_t vertexBufferOffset{};
        uint32_t vertexCount{};
        uint32_t vertexStride{};
        D3DBuffer *indexBuffer{};
        int indexSize{};
        size_t indexBufferOffset{};
        uint32_t indexCount{};
        D3DBuffer *transformBuffer{};
        size_t transformBufferOffset{};
        QString primitiveCount;
        QString primitiveOffset;
    };

    struct D3DInstance
    {
        bool operator==(const D3DInstance &) const = default;

        ItemId itemId{};
        QString transform;
        std::vector<D3DGeometry> geometries;
    };

    D3DGeometry &getGeometry(int instanceIndex, int geometryIndex);

    ItemId mItemId{};
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
    std::vector<D3DInstance> mInstances;
};

#endif // _WIN32
