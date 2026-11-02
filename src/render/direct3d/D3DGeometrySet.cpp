#include "D3DGeometrySet.h"

namespace {
    ComPtr<ID3D12Resource> createBuffer(ID3D12Device &device, UINT64 size,
        D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
    {
        const auto heapProperties = CD3DX12_HEAP_PROPERTIES(heapType);
        const auto desc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);
        auto resource = ComPtr<ID3D12Resource>();
        if (FAILED(device.CreateCommittedResource(&heapProperties,
                D3D12_HEAP_FLAG_NONE, &desc, initialState, nullptr,
                IID_PPV_ARGS(&resource))))
            return { };
        return resource;
    }
} // namespace

D3DGeometrySet::D3DGeometrySet(const GeometrySet &geometrySet)
    : mItemId(geometrySet.id)
{
    mUsedItems += geometrySet.id;
    for (auto item : geometrySet.items)
        if (auto geometry = castItem<Geometry>(item))
            mGeometries.push_back({
                .itemId = geometry->id,
                .type = geometry->geometryType,
                .primitiveCount = geometry->count,
                .primitiveOffset = geometry->offset,
            });
}

bool D3DGeometrySet::operator==(const D3DGeometrySet &rhs) const
{
    return std::tie(mGeometries) == std::tie(rhs.mGeometries);
}

auto D3DGeometrySet::getGeometry(int geometryIndex) -> D3DGeometry &
{
    Q_ASSERT(geometryIndex >= 0
        && geometryIndex < static_cast<int>(mGeometries.size()));
    return mGeometries[geometryIndex];
}

void D3DGeometrySet::setVertexBuffer(int geometryIndex, D3DBuffer *buffer,
    const Block &block, D3DRenderSession &renderSession)
{
    if (!buffer)
        return;

    mUsedItems += buffer->usedItems();
    auto &geometry = getGeometry(geometryIndex);
    geometry.vertexStride = getBlockStride(block);
    if (geometry.type == Geometry::GeometryType::AxisAlignedBoundingBoxes) {
        const auto expectedStride = sizeof(VkAabbPositionsKHR);
        if (geometry.vertexStride != expectedStride) {
            mMessages.insert(geometry.itemId,
                MessageType::InvalidGeometryStride,
                QStringLiteral("%1/%2 bytes")
                    .arg(geometry.vertexStride)
                    .arg(expectedStride));
            return;
        }
    }

    auto offset = 0;
    auto rowCount = 0;
    renderSession.evaluateBlockProperties(block, &offset, &rowCount);
    geometry.vertexBuffer = buffer;
    geometry.vertexBufferOffset = static_cast<size_t>(offset);
    geometry.vertexCount = static_cast<uint32_t>(rowCount);
}

void D3DGeometrySet::setIndexBuffer(int geometryIndex, D3DBuffer *buffer,
    const Block &block, D3DRenderSession &renderSession)
{
    if (!buffer)
        return;

    mUsedItems += buffer->usedItems();
    auto offset = 0;
    auto rowCount = 0;
    renderSession.evaluateBlockProperties(block, &offset, &rowCount);

    auto indexSize = 0;
    for (auto item : block.items)
        if (auto field = castItem<Field>(item)) {
            indexSize += getFieldSize(*field);
            mUsedItems += field->id;
        }

    auto &geometry = getGeometry(geometryIndex);
    geometry.indexBuffer = buffer;
    geometry.indexBufferOffset = static_cast<size_t>(offset);
    geometry.indexSize = indexSize;
    if (indexSize != 2 && indexSize != 4) {
        mMessages.insert(block.id, MessageType::InvalidIndexType,
            QStringLiteral("%1 bytes").arg(indexSize));
        geometry.indexBuffer = nullptr;
        return;
    }
    const auto indicesPerRow = getBlockStride(block) / indexSize;
    geometry.indexCount = static_cast<uint32_t>(rowCount) * indicesPerRow;
}

void D3DGeometrySet::setTransformBuffer(int geometryIndex, D3DBuffer *buffer,
    const Block &block, D3DRenderSession &renderSession)
{
    if (!buffer)
        return;

    mUsedItems += buffer->usedItems();
    auto offset = 0;
    auto rowCount = 0;
    renderSession.evaluateBlockProperties(block, &offset, &rowCount);

    auto &geometry = getGeometry(geometryIndex);
    geometry.transformBuffer = buffer;
    geometry.transformBufferOffset = static_cast<size_t>(offset);
}

bool D3DGeometrySet::build(D3DContext &context, ScriptEngine &scriptEngine)
{
    if (mBottomLevel)
        return true;

    auto device = ComPtr<ID3D12Device5>();
    auto commandList = ComPtr<ID3D12GraphicsCommandList4>();
    if (FAILED(context.device.QueryInterface(IID_PPV_ARGS(&device)))
        || FAILED(context.graphicsCommandList.As(&commandList)))
        return false;

    auto geometryDescs = std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>();
    for (const auto &geometry : mGeometries) {
        mUsedItems += geometry.itemId;
        if (!geometry.vertexBuffer) {
            mMessages.insert(geometry.itemId, MessageType::BufferNotSet,
                "Vertices");
            return false;
        }

        geometry.vertexBuffer->prepareAccelerationStructureGeometry(context);
        const auto primitiveOffset = scriptEngine.evaluateUInt(
            geometry.primitiveOffset, geometry.itemId);
        auto desc = D3D12_RAYTRACING_GEOMETRY_DESC{
            .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
        };

        if (geometry.type
            == Geometry::GeometryType::AxisAlignedBoundingBoxes) {
            const auto maxPrimitiveCount = geometry.vertexCount;
            const auto primitiveCount = (geometry.primitiveCount.isEmpty()
                    ? maxPrimitiveCount
                    : scriptEngine.evaluateUInt(geometry.primitiveCount,
                          geometry.itemId));
            if (primitiveCount > maxPrimitiveCount) {
                mMessages.insert(geometry.itemId, MessageType::CountExceeded,
                    QStringLiteral("%1 > %2")
                        .arg(primitiveCount)
                        .arg(maxPrimitiveCount));
                return false;
            }
            desc.Type =
                D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
            desc.AABBs = {
                .AABBCount = primitiveCount,
                .AABBs = {
                    .StartAddress = geometry.vertexBuffer->getDeviceAddress()
                        + geometry.vertexBufferOffset + primitiveOffset,
                    .StrideInBytes = geometry.vertexStride,
                },
            };
        } else {
            auto maxPrimitiveCount = geometry.vertexCount / 3;
            auto indexFormat = DXGI_FORMAT_UNKNOWN;
            auto indexAddress = D3D12_GPU_VIRTUAL_ADDRESS{ };
            if (geometry.indexBuffer) {
                geometry.indexBuffer->prepareAccelerationStructureGeometry(
                    context);
                maxPrimitiveCount = geometry.indexCount / 3;
                indexFormat = (geometry.indexSize == 2 ? DXGI_FORMAT_R16_UINT
                                                       : DXGI_FORMAT_R32_UINT);
                indexAddress = geometry.indexBuffer->getDeviceAddress()
                    + geometry.indexBufferOffset + primitiveOffset;
            }
            const auto primitiveCount = (geometry.primitiveCount.isEmpty()
                    ? maxPrimitiveCount
                    : scriptEngine.evaluateUInt(geometry.primitiveCount,
                          geometry.itemId));
            if (primitiveCount > maxPrimitiveCount) {
                mMessages.insert(geometry.itemId, MessageType::CountExceeded,
                    QStringLiteral("%1 > %2")
                        .arg(primitiveCount)
                        .arg(maxPrimitiveCount));
                return false;
            }
            if (geometry.transformBuffer)
                geometry.transformBuffer->prepareAccelerationStructureGeometry(
                    context);
            desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            desc.Triangles = {
                .Transform3x4 = (geometry.transformBuffer
                        ? geometry.transformBuffer->getDeviceAddress()
                            + geometry.transformBufferOffset
                        : 0),
                .IndexFormat = indexFormat,
                .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
                .IndexCount = geometry.indexBuffer ? primitiveCount * 3 : 0,
                .VertexCount = (geometry.indexBuffer ? geometry.vertexCount
                                                     : primitiveCount * 3),
                .IndexBuffer = indexAddress,
                .VertexBuffer = {
                    .StartAddress = geometry.vertexBuffer->getDeviceAddress()
                        + geometry.vertexBufferOffset
                        + (geometry.indexBuffer ? 0 : primitiveOffset),
                    .StrideInBytes = geometry.vertexStride,
                },
            };
        }
        geometryDescs.push_back(desc);
    }

    if (geometryDescs.empty())
        return false;

    const auto inputs = D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS{
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
        .Flags =
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
        .NumDescs = static_cast<UINT>(geometryDescs.size()),
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .pGeometryDescs = geometryDescs.data(),
    };
    auto info = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO{ };
    device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
    mBottomLevel = createBuffer(*device.Get(), info.ResultDataMaxSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    mScratchBuffer = createBuffer(*device.Get(), info.ScratchDataSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    if (!mBottomLevel || !mScratchBuffer) {
        mBottomLevel.Reset();
        mScratchBuffer.Reset();
        return false;
    }

    const auto buildDesc =
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC{
            .DestAccelerationStructureData =
                mBottomLevel->GetGPUVirtualAddress(),
            .Inputs = inputs,
            .ScratchAccelerationStructureData =
                mScratchBuffer->GetGPUVirtualAddress(),
        };
    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(mBottomLevel.Get());
    commandList->ResourceBarrier(1, &barrier);
    return true;
}
