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

D3DGeometrySet::D3DGeometrySet(const Item &geometrySource)
    : mItemId(geometrySource.id)
{
    mUsedItems += geometrySource.id;
    for (auto item : geometrySource.items)
        if (auto geometry = castItem<Geometry>(item))
            mGeometries.push_back({
                .itemId = geometry->id,
                .type = geometry->geometryType,
                .opaque = geometry->opaque,
                .noDuplicateAnyHitInvocation =
                    geometry->noDuplicateAnyHitInvocation,
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
    auto device = ComPtr<ID3D12Device5>();
    auto commandList = ComPtr<ID3D12GraphicsCommandList4>();
    if (FAILED(context.device.QueryInterface(IID_PPV_ARGS(&device)))
        || FAILED(context.graphicsCommandList.As(&commandList)))
        return false;

    auto geometryDescs = std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>();
    auto inputKey = std::vector<UINT64>{
        static_cast<UINT64>(mGeometries.size())
    };
    auto updateKey = inputKey;
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
        auto geometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
        if (geometry.opaque)
            geometryFlags |= D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        if (geometry.noDuplicateAnyHitInvocation)
            geometryFlags |=
                D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;
        auto desc = D3D12_RAYTRACING_GEOMETRY_DESC{
            .Flags = geometryFlags,
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

        const auto append = [](auto &key,
                                std::initializer_list<UINT64> values) {
            key.insert(key.end(), values.begin(), values.end());
        };
        append(inputKey, { static_cast<UINT64>(desc.Type),
                             static_cast<UINT64>(desc.Flags) });
        append(updateKey, { static_cast<UINT64>(desc.Type),
                              static_cast<UINT64>(desc.Flags) });
        if (desc.Type
            == D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS) {
            const auto values = std::initializer_list<UINT64>{
                desc.AABBs.AABBCount, desc.AABBs.AABBs.StartAddress,
                desc.AABBs.AABBs.StrideInBytes
            };
            append(inputKey, values);
            append(updateKey, values);
            inputKey.push_back(geometry.vertexBuffer->contentRevision());
        } else {
            const auto &triangles = desc.Triangles;
            const auto values = std::initializer_list<UINT64>{
                triangles.Transform3x4,
                static_cast<UINT64>(triangles.IndexFormat),
                static_cast<UINT64>(triangles.VertexFormat),
                triangles.IndexCount, triangles.VertexCount,
                triangles.IndexBuffer, triangles.VertexBuffer.StartAddress,
                triangles.VertexBuffer.StrideInBytes
            };
            append(inputKey, values);
            append(updateKey, values);
            inputKey.push_back(geometry.vertexBuffer->contentRevision());
            const auto indexRevision = geometry.indexBuffer
                ? geometry.indexBuffer->contentRevision()
                : 0;
            inputKey.push_back(indexRevision);
            updateKey.push_back(indexRevision);
            inputKey.push_back(geometry.transformBuffer
                    ? geometry.transformBuffer->contentRevision()
                    : 0);
        }
    }

    if (geometryDescs.empty())
        return false;

    if (mBottomLevel && mInputKey == inputKey)
        return true;

    const auto compatible = mBottomLevel && mUpdateKey == updateKey;
    const auto update = compatible && mUpdateCapable;
    const auto updateCapable = mUpdateCapable || compatible;

    auto inputs = D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS{
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
        .Flags =
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
        .NumDescs = static_cast<UINT>(geometryDescs.size()),
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .pGeometryDescs = geometryDescs.data(),
    };
    if (updateCapable)
        inputs.Flags |=
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    if (update)
        inputs.Flags |=
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;

    auto info = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO{ };
    if (update) {
        info.ScratchDataSizeInBytes = mUpdateScratchSize;
    } else {
        device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
    }

    auto bottomLevel = update
        ? mBottomLevel
        : createBuffer(*device.Get(), info.ResultDataMaxSizeInBytes,
              D3D12_HEAP_TYPE_DEFAULT,
              D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
              D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto scratchBuffer = createBuffer(*device.Get(),
        info.ScratchDataSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    if (!bottomLevel || !scratchBuffer)
        return false;

    if (update) {
        const auto barrier =
            CD3DX12_RESOURCE_BARRIER::UAV(bottomLevel.Get());
        commandList->ResourceBarrier(1, &barrier);
    }

    const auto buildDesc =
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC{
            .DestAccelerationStructureData =
                bottomLevel->GetGPUVirtualAddress(),
            .Inputs = inputs,
            .SourceAccelerationStructureData = update
                ? mBottomLevel->GetGPUVirtualAddress()
                : D3D12_GPU_VIRTUAL_ADDRESS{},
            .ScratchAccelerationStructureData =
                scratchBuffer->GetGPUVirtualAddress(),
        };
    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(bottomLevel.Get());
    commandList->ResourceBarrier(1, &barrier);

    context.stagingBuffers.push_back(std::move(scratchBuffer));
    if (!update) {
        if (mBottomLevel)
            context.stagingBuffers.push_back(std::move(mBottomLevel));
        mBottomLevel = std::move(bottomLevel);
        mUpdateScratchSize = info.UpdateScratchDataSizeInBytes;
        mUpdateCapable = updateCapable;
    }
    mInputKey = std::move(inputKey);
    mUpdateKey = std::move(updateKey);
    ++mRevision;
    return true;
}
