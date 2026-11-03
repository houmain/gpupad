#pragma once
#if defined(D3D_ENABLED)

#  include "D3DBuffer.h"
#  include "scripting/ScriptEngine.h"

class D3DGeometrySet
{
public:
    explicit D3DGeometrySet(const Item &geometrySource);
    bool operator==(const D3DGeometrySet &rhs) const;

    void setVertexBuffer(int geometryIndex, D3DBuffer *buffer,
        const Block &block, D3DRenderSession &renderSession);
    void setIndexBuffer(int geometryIndex, D3DBuffer *buffer,
        const Block &block, D3DRenderSession &renderSession);
    void setTransformBuffer(int geometryIndex, D3DBuffer *buffer,
        const Block &block, D3DRenderSession &renderSession);
    bool build(D3DContext &context, ScriptEngine &scriptEngine);

    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress() const
    {
        return (mBottomLevel ? mBottomLevel->GetGPUVirtualAddress() : 0);
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

    D3DGeometry &getGeometry(int geometryIndex);

    ItemId mItemId{ };
    MessageList mMessages;
    QSet<ItemId> mUsedItems;
    std::vector<D3DGeometry> mGeometries;
    ComPtr<ID3D12Resource> mBottomLevel;
    ComPtr<ID3D12Resource> mScratchBuffer;
};

#endif // D3D_ENABLED
