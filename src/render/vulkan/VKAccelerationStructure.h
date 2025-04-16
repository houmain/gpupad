#pragma once

#include "VKBuffer.h"
#include "scripting/ScriptEngine.h"

class VKAccelerationStructure
{
public:
    VKAccelerationStructure(const AccelerationStructure &accelStruct,
        ScriptEngine &scriptEngine);
    bool operator==(const VKAccelerationStructure &rhs) const;
    void setVertexBuffer(int instanceIndex, int geometryIndex, VKBuffer *buffer,
        const Block &block, VKRenderSession &renderSession);
    void setIndexBuffer(int instanceIndex, int geometryIndex, VKBuffer *buffer,
        const Block &block, VKRenderSession &renderSession);

    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    const KDGpu::AccelerationStructure &topLevelAs() const
    {
        return mTopLevelAs;
    }

    void prepare(VKContext &context);

private:
    struct VKGeometry
    {
        bool operator==(const VKGeometry &) const = default;

        ItemId itemId{};
        Geometry::GeometryType type{};
        VKBuffer *vertexBuffer{};
        size_t vertexBufferOffset{};
        uint32_t maxVertexIndex{};
        uint32_t vertexStride{};
        VKBuffer *indexBuffer{};
        KDGpu::IndexType indexType{};
        size_t indexBufferOffset{};
        uint32_t primitiveCount{};
        uint32_t primitiveOffset{};
    };

    struct VKInstance
    {
        bool operator==(const VKInstance &) const = default;

        ItemId itemId{};
        ScriptValueList transform;
        std::vector<VKGeometry> geometries;
    };

    VKGeometry &getGeometry(int instanceIndex, int geometryIndex);
    void memoryBarrier(KDGpu::CommandRecorder &commandRecorder);

    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
    std::vector<VKInstance> mInstances;
    std::vector<KDGpu::AccelerationStructure> mBottomLevelAs;
    KDGpu::AccelerationStructure mTopLevelAs;
};
