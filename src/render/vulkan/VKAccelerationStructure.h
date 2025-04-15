#pragma once

#include "VKItem.h"
#include "VKBuffer.h"
#include "scripting/ScriptEngine.h"

class VKAccelerationStructure
{
public:
    VKAccelerationStructure(const AccelerationStructure &accelStruct,
        ScriptEngine &scriptEngine);
    bool operator==(const VKAccelerationStructure &rhs) const;
    void setBuffers(int index, VKBuffer *vertexBuffer, VKBuffer *indexBuffer);

    ItemId itemId() const { return mItemId; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    const KDGpu::AccelerationStructure &topLevelAs() const
    {
        return mTopLevelAs;
    }

    void prepare(VKContext &context);

private:
    struct VKInstance
    {
        ScriptValueList transform;
        Instance::GeometryType geometryType{};
        VKBuffer *vertexBuffer{};
        VKBuffer *indexBuffer{};
    };
    friend bool operator==(const VKInstance &a, const VKInstance &b);
    void createBottomLevelAsAabbs(VKContext &context,
        const VKInstance &instance);
    void createBottomLevelAsTriangles(VKContext &context,
        const VKInstance &instance);
    void createTopLevelAsInstances(VKContext &context);
    void memoryBarrier(KDGpu::CommandRecorder &commandRecorder);

    ItemId mItemId{};
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
    std::vector<VKInstance> mInstances;
    std::vector<KDGpu::AccelerationStructure> mBottomLevelAs;
    KDGpu::AccelerationStructure mTopLevelAs;
};
