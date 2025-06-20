#pragma once

#include "VKBuffer.h"
#include "scripting/ScriptEngine.h"

class VKAccelerationStructure
{
public:
    explicit VKAccelerationStructure(const AccelerationStructure &accelStruct);
    bool operator==(const VKAccelerationStructure &rhs) const;
    void setVertexBuffer(int instanceIndex, int geometryIndex, VKBuffer *buffer,
        const Block &block, VKRenderSession &renderSession);
    void setIndexBuffer(int instanceIndex, int geometryIndex, VKBuffer *buffer,
        const Block &block, VKRenderSession &renderSession);
    void setTransformBuffer(int instanceIndex, int geometryIndex,
        VKBuffer *buffer, const Block &block, VKRenderSession &renderSession);

    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    const KDGpu::AccelerationStructure &topLevelAs() const
    {
        return mTopLevelAs;
    }

    void prepare(VKContext &context, ScriptEngine &scriptEngine);

private:
    struct VKGeometry
    {
        bool operator==(const VKGeometry &) const = default;

        ItemId itemId{};
        Geometry::GeometryType type{};
        VKBuffer *vertexBuffer{};
        size_t vertexBufferOffset{};
        uint32_t vertexCount{};
        uint32_t vertexStride{};
        VKBuffer *indexBuffer{};
        KDGpu::IndexType indexType{ KDGpu::IndexType::None };
        size_t indexBufferOffset{};
        uint32_t indexCount{};
        VKBuffer *transformBuffer{};
        size_t transformBufferOffset{};
        QString primitiveCount;
        QString primitiveOffset;
    };

    struct VKInstance
    {
        bool operator==(const VKInstance &) const = default;

        ItemId itemId{};
        QString transform;
        std::vector<VKGeometry> geometries;
    };

    VKGeometry &getGeometry(int instanceIndex, int geometryIndex);
    void memoryBarrier(KDGpu::CommandRecorder &commandRecorder);

    ItemId mItemId{};
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
    std::vector<VKInstance> mInstances;
    std::vector<KDGpu::AccelerationStructure> mBottomLevelAs;
    KDGpu::AccelerationStructure mTopLevelAs;
};
