#pragma once
#if defined(D3D_ENABLED)

#  include "D3DBuffer.h"
#  include "scripting/ScriptEngine.h"

class D3DAccelerationStructure
{
public:
    explicit D3DAccelerationStructure(const AccelerationStructure &accelStruct);
    bool operator==(const D3DAccelerationStructure &rhs) const;

    void setVertexBuffer(int instanceIndex, int geometryIndex,
        D3DBuffer *buffer, const Block &block, D3DRenderSession &renderSession);
    void setIndexBuffer(int instanceIndex, int geometryIndex, D3DBuffer *buffer,
        const Block &block, D3DRenderSession &renderSession);
    void setTransformBuffer(int instanceIndex, int geometryIndex,
        D3DBuffer *buffer, const Block &block, D3DRenderSession &renderSession);
    bool build(D3DContext &context, ScriptEngine &scriptEngine);

    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress() const
    {
        return (mTopLevel ? mTopLevel->GetGPUVirtualAddress() : 0);
    }

    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    struct D3DGeometry
    {
        bool operator==(const D3DGeometry &) const = default;

        ItemId itemId{ };
        Geometry::GeometryType type{ };
        D3DBuffer *vertexBuffer{ };
        size_t vertexBufferOffset{ };
        uint32_t vertexCount{ };
        uint32_t vertexStride{ };
        D3DBuffer *indexBuffer{ };
        int indexSize{ };
        size_t indexBufferOffset{ };
        uint32_t indexCount{ };
        D3DBuffer *transformBuffer{ };
        size_t transformBufferOffset{ };
        QString primitiveCount;
        QString primitiveOffset;
    };

    struct D3DInstance
    {
        bool operator==(const D3DInstance &) const = default;

        ItemId itemId{ };
        QString transform;
        ItemId sourceInstanceId{ };
        QString customIndex;
        QString mask;
        QString hitGroupOffset;
    };

    // geometry inputs belong to the BLAS owner
    struct D3DBottomLevelInputs
    {
        bool operator==(const D3DBottomLevelInputs &) const = default;

        std::vector<D3DGeometry> geometries;
    };

    struct BottomLevelUpdateState
    {
        std::vector<UINT64> key;
        UINT64 resultSize{ };
        UINT64 scratchSize{ };
    };

    D3DGeometry &getGeometry(int instanceIndex, int geometryIndex);
    bool buildBottomLevels(D3DContext &context, ScriptEngine &scriptEngine);

    ItemId mItemId{ };
    bool mAllowTlasUpdate{ };
    bool mAllowBlasUpdate{ };
    MessageList mMessages;
    QSet<ItemId> mUsedItems;
    std::vector<D3DInstance> mInstances;
    std::vector<D3DBottomLevelInputs> mBottomLevelInputs;
    // referencing instances share the owner's allocation
    std::vector<ComPtr<ID3D12Resource>> mBottomLevels;
    // retain old BLAS allocations until the replacement TLAS is recorded
    std::vector<ComPtr<ID3D12Resource>> mReplacedBottomLevels;
    std::vector<std::vector<UINT64>> mBottomLevelKeys;
    std::vector<BottomLevelUpdateState> mBottomLevelUpdates;
    ComPtr<ID3D12Resource> mInstanceBuffer;
    ComPtr<ID3D12Resource> mTopLevel;
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> mLastInstanceDescs;
    UINT64 mTopLevelResultSize{ };
    UINT64 mTopLevelUpdateScratchSize{ };
    bool mTopLevelDirty{ };
};

#endif // D3D_ENABLED
