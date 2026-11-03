#pragma once

#include "VKBuffer.h"
#include "scripting/ScriptEngine.h"

class VKGeometrySet
{
public:
    explicit VKGeometrySet(const Item &geometrySource);
    bool operator==(const VKGeometrySet &rhs) const;

    void setVertexBuffer(int geometryIndex, VKBuffer *buffer,
        const Block &block, VKRenderSession &renderSession);
    void setIndexBuffer(int geometryIndex, VKBuffer *buffer,
        const Block &block, VKRenderSession &renderSession);
    void setTransformBuffer(int geometryIndex, VKBuffer *buffer,
        const Block &block, VKRenderSession &renderSession);
    void prepare(VKContext &context, ScriptEngine &scriptEngine);

    const KDGpu::AccelerationStructure &bottomLevelAs() const
    {
        return mBottomLevelAs;
    }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    struct VKGeometry
    {
        bool operator==(const VKGeometry &) const = default;

        ItemId itemId{ };
        Geometry::GeometryType type{ };
        VKBuffer *vertexBuffer{ };
        size_t vertexBufferOffset{ };
        uint32_t vertexCount{ };
        uint32_t vertexStride{ };
        VKBuffer *indexBuffer{ };
        KDGpu::IndexType indexType{ KDGpu::IndexType::None };
        size_t indexBufferOffset{ };
        uint32_t indexCount{ };
        VKBuffer *transformBuffer{ };
        size_t transformBufferOffset{ };
        QString primitiveCount;
        QString primitiveOffset;
    };

    VKGeometry &getGeometry(int geometryIndex);
    void memoryBarrier(KDGpu::CommandRecorder &commandRecorder);

    ItemId mItemId{ };
    MessageList mMessages;
    QSet<ItemId> mUsedItems;
    std::vector<VKGeometry> mGeometries;
    KDGpu::AccelerationStructure mBottomLevelAs;
};
