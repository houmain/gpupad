#pragma once

#include "../AccelerationStructureBase.h"
#include "VKGeometrySet.h"

class VKAccelerationStructure : public AccelerationStructureBase
{
public:
    explicit VKAccelerationStructure(const AccelerationStructure &accelStruct);
    bool operator==(const VKAccelerationStructure &rhs) const;
    void setGeometrySet(int instanceIndex, VKGeometrySet *geometrySet);
    template <typename GeometrySets>
    void relinkGeometrySets(GeometrySets &geometrySets)
    {
        for (auto &instance : mInstances) {
            const auto it = geometrySets.find(instance.geometrySetId);
            instance.geometrySet =
                (it == geometrySets.end() ? nullptr : &it->second);
        }
    }

    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    const KDGpu::AccelerationStructure &topLevelAs() const
    {
        return mTopLevelAs;
    }
    void prepare(VKContext &context, ScriptEngine &scriptEngine);

private:
    struct VKInstance
    {
        bool operator==(const VKInstance &rhs) const
        {
            return std::tie(itemId, geometrySetId, transform)
                == std::tie(rhs.itemId, rhs.geometrySetId, rhs.transform);
        }

        ItemId itemId{ };
        ItemId geometrySetId{ };
        QString transform;
        VKGeometrySet *geometrySet{ };
    };

    void memoryBarrier(KDGpu::CommandRecorder &commandRecorder,
        KDGpu::PipelineStageFlags destinationStages);

    ItemId mItemId{ };
    MessageList mMessages;
    QSet<ItemId> mUsedItems;
    std::vector<VKInstance> mInstances;
    KDGpu::AccelerationStructure mTopLevelAs;
};
