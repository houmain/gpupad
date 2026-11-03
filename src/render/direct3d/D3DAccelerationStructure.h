#pragma once
#if defined(D3D_ENABLED)

#  include "../AccelerationStructureBase.h"
#  include "D3DGeometrySet.h"

class D3DAccelerationStructure : public AccelerationStructureBase
{
public:
    explicit D3DAccelerationStructure(const AccelerationStructure &accelStruct);
    bool operator==(const D3DAccelerationStructure &rhs) const;

    void setGeometrySet(int instanceIndex, D3DGeometrySet *geometrySet);
    template <typename GeometrySets>
    void relinkGeometrySets(GeometrySets &geometrySets)
    {
        for (auto &instance : mInstances) {
            const auto it = geometrySets.find(instance.geometrySetId);
            instance.geometrySet =
                (it == geometrySets.end() ? nullptr : &it->second);
        }
    }
    bool build(D3DContext &context, ScriptEngine &scriptEngine);

    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress() const
    {
        return (mTopLevel ? mTopLevel->GetGPUVirtualAddress() : 0);
    }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    struct D3DInstance
    {
        bool operator==(const D3DInstance &rhs) const
        {
            return std::tie(itemId, geometrySetId, transform, shaderInstanceId,
                       visibilityMask, hitGroupRecordOffset, triangleCulling,
                       frontFace, opacityMode)
                == std::tie(rhs.itemId, rhs.geometrySetId, rhs.transform,
                    rhs.shaderInstanceId, rhs.visibilityMask,
                    rhs.hitGroupRecordOffset, rhs.triangleCulling,
                    rhs.frontFace, rhs.opacityMode);
        }

        ItemId itemId{ };
        ItemId geometrySetId{ };
        QString transform;
        QString shaderInstanceId;
        QString visibilityMask;
        QString hitGroupRecordOffset;
        bool triangleCulling{ };
        Instance::FrontFace frontFace{ };
        Instance::OpacityMode opacityMode{ };
        D3DGeometrySet *geometrySet{ };
    };

    ItemId mItemId{ };
    MessageList mMessages;
    QSet<ItemId> mUsedItems;
    std::vector<D3DInstance> mInstances;
    ComPtr<ID3D12Resource> mInstanceBuffer;
    ComPtr<ID3D12Resource> mTopLevel;
    ComPtr<ID3D12Resource> mScratchBuffer;
};

#endif // D3D_ENABLED
