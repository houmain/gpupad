
#include "D3DAccelerationStructure.h"
#include <algorithm>
#include <cmath>

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

D3DAccelerationStructure::D3DAccelerationStructure(
    const AccelerationStructure &accelStruct)
    : mItemId(accelStruct.id)
    , mAllowTlasUpdate(accelStruct.allowTlasUpdate)
{
    mUsedItems += accelStruct.id;

    for (auto item : accelStruct.items)
        if (auto instance = castItem<Instance>(item)) {
            auto geometries = std::vector<D3DGeometry>();
            for (auto item : instance->items)
                if (auto geometry = castItem<Geometry>(item)) {
                    geometries.push_back({
                        .itemId = geometry->id,
                        .type = geometry->geometryType,
                        .primitiveCount = geometry->count,
                        .primitiveOffset = geometry->offset,
                    });
                }
            mInstances.push_back({
                .itemId = item->id,
                .transform = instance->transform,
                .sourceInstanceId = instance->sourceInstanceId,
                .customIndex = instance->customIndex,
                .mask = instance->mask,
                .hitGroupOffset = instance->hitGroupOffset,
            });
            mBottomLevelInputs.push_back({
                .geometries = std::move(geometries),
            });
        }
}

bool D3DAccelerationStructure::operator==(
    const D3DAccelerationStructure &rhs) const
{
    return std::tie(mAllowTlasUpdate, mInstances, mBottomLevelInputs)
        == std::tie(rhs.mAllowTlasUpdate, rhs.mInstances,
            rhs.mBottomLevelInputs);
}

auto D3DAccelerationStructure::getGeometry(int instanceIndex, int geometryIndex)
    -> D3DGeometry &
{
    Q_ASSERT(instanceIndex >= 0
        && instanceIndex < static_cast<int>(mInstances.size()));
    auto &instance = mBottomLevelInputs[instanceIndex];
    Q_ASSERT(geometryIndex >= 0
        && geometryIndex < static_cast<int>(instance.geometries.size()));
    return instance.geometries[geometryIndex];
}

void D3DAccelerationStructure::setVertexBuffer(int instanceIndex,
    int geometryIndex, D3DBuffer *buffer, const Block &block,
    D3DRenderSession &renderSession)
{
    if (!buffer)
        return;

    mUsedItems += buffer->usedItems();

    auto &geometry = getGeometry(instanceIndex, geometryIndex);
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

void D3DAccelerationStructure::setIndexBuffer(int instanceIndex,
    int geometryIndex, D3DBuffer *buffer, const Block &block,
    D3DRenderSession &renderSession)
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

    auto &geometry = getGeometry(instanceIndex, geometryIndex);
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

void D3DAccelerationStructure::setTransformBuffer(int instanceIndex,
    int geometryIndex, D3DBuffer *buffer, const Block &block,
    D3DRenderSession &renderSession)
{
    if (!buffer)
        return;

    mUsedItems += buffer->usedItems();

    auto offset = 0;
    auto rowCount = 0;
    renderSession.evaluateBlockProperties(block, &offset, &rowCount);

    auto &geometry = getGeometry(instanceIndex, geometryIndex);
    geometry.transformBuffer = buffer;
    geometry.transformBufferOffset = static_cast<size_t>(offset);
}

bool D3DAccelerationStructure::buildBottomLevels(D3DContext &context,
    ScriptEngine &scriptEngine)
{
    auto device = ComPtr<ID3D12Device5>();
    auto commandList = ComPtr<ID3D12GraphicsCommandList4>();
    if (FAILED(context.device.QueryInterface(IID_PPV_ARGS(&device)))
        || FAILED(context.graphicsCommandList.As(&commandList)))
        return false;

    mBottomLevels.clear();
    mBottomLevels.resize(mInstances.size());
    mScratchBuffers.clear();

    for (auto instanceIndex = 0u; instanceIndex < mInstances.size();
        ++instanceIndex) {
        const auto &instance = mInstances[instanceIndex];
        mUsedItems += instance.itemId;

        const auto &geometries = mBottomLevelInputs[instanceIndex].geometries;
        if (instance.sourceInstanceId) {
            if (!geometries.empty()) {
                mMessages.insert(instance.itemId, MessageType::ScriptError,
                    "A shared instance must not also own geometry");
                return false;
            }
            continue;
        }

        auto geometryDescs = std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>();
        for (const auto &geometry : geometries) {
            mUsedItems += geometry.itemId;
            if (!geometry.vertexBuffer) {
                mMessages.insert(geometry.itemId, MessageType::BufferNotSet,
                    "Vertices");
                return false;
            }

            geometry.vertexBuffer->prepareAccelerationStructureGeometry(
                context);
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
                    mMessages.insert(geometry.itemId,
                        MessageType::CountExceeded,
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
                    indexFormat = (geometry.indexSize == 2
                            ? DXGI_FORMAT_R16_UINT
                            : DXGI_FORMAT_R32_UINT);
                    indexAddress = geometry.indexBuffer->getDeviceAddress()
                        + geometry.indexBufferOffset + primitiveOffset;
                }
                const auto primitiveCount = (geometry.primitiveCount.isEmpty()
                        ? maxPrimitiveCount
                        : scriptEngine.evaluateUInt(geometry.primitiveCount,
                              geometry.itemId));
                if (primitiveCount > maxPrimitiveCount) {
                    mMessages.insert(geometry.itemId,
                        MessageType::CountExceeded,
                        QStringLiteral("%1 > %2")
                            .arg(primitiveCount)
                            .arg(maxPrimitiveCount));
                    return false;
                }
                if (geometry.transformBuffer)
                    geometry.transformBuffer
                        ->prepareAccelerationStructureGeometry(context);
                desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
                desc.Triangles = {
                    .Transform3x4 = (geometry.transformBuffer
                            ? geometry.transformBuffer->getDeviceAddress()
                                + geometry.transformBufferOffset
                            : 0),
                    .IndexFormat = indexFormat,
                    .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
                    .IndexCount = (geometry.indexBuffer
                            ? primitiveCount * 3
                            : 0),
                    .VertexCount = (geometry.indexBuffer
                            ? geometry.vertexCount
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
            continue;

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
        auto bottomLevel = createBuffer(*device.Get(),
            info.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto scratch = createBuffer(*device.Get(), info.ScratchDataSizeInBytes,
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        if (!bottomLevel || !scratch)
            return false;

        const auto buildDesc =
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC{
                .DestAccelerationStructureData =
                    bottomLevel->GetGPUVirtualAddress(),
                .Inputs = inputs,
                .ScratchAccelerationStructureData =
                    scratch->GetGPUVirtualAddress(),
            };
        commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0,
            nullptr);

        const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(bottomLevel.Get());
        commandList->ResourceBarrier(1, &barrier);

        mBottomLevels[instanceIndex] = std::move(bottomLevel);
        mScratchBuffers.push_back(std::move(scratch));
    }

    mBottomLevelsBuilt = true;
    return true;
}

bool D3DAccelerationStructure::build(D3DContext &context,
    ScriptEngine &scriptEngine)
{
    if (!mBottomLevelsBuilt && !buildBottomLevels(context, scriptEngine))
        return false;

    auto device = ComPtr<ID3D12Device5>();
    auto commandList = ComPtr<ID3D12GraphicsCommandList4>();
    if (FAILED(context.device.QueryInterface(IID_PPV_ARGS(&device)))
        || FAILED(context.graphicsCommandList.As(&commandList)))
        return false;
    auto instanceDescs = std::vector<D3D12_RAYTRACING_INSTANCE_DESC>();

    for (auto instanceIndex = 0u; instanceIndex < mInstances.size();
        ++instanceIndex) {
        const auto &instance = mInstances[instanceIndex];
        auto ownerIndex = instanceIndex;
        if (instance.sourceInstanceId) {
            const auto owner = std::find_if(mInstances.begin(),
                mInstances.end(), [&](const auto &value) {
                    return value.itemId == instance.sourceInstanceId;
                });
            if (owner == mInstances.end() || owner->sourceInstanceId) {
                mMessages.insert(instance.itemId, MessageType::ScriptError,
                    "Geometry source must be a geometry-owning instance in the "
                    "same acceleration structure (no chains)");
                return false;
            }
            ownerIndex = static_cast<unsigned>(owner - mInstances.begin());
            mUsedItems += owner->itemId;
        }
        const auto &bottomLevel = mBottomLevels[ownerIndex];
        if (!bottomLevel) {
            if (!instance.sourceInstanceId)
                continue; // preserve existing empty-instance behavior
            mMessages.insert(instance.itemId, MessageType::ScriptError,
                "Geometry source has no built BLAS");
            return false;
        }

        const auto readField = [&](const QString &expression, UINT maximum,
                                   UINT &out) {
            const auto value =
                scriptEngine.evaluateValue(expression, instance.itemId);
            if (!std::isfinite(value) || value < 0 || value > maximum
                || std::floor(value) != value) {
                mMessages.insert(instance.itemId, MessageType::ScriptError,
                    QStringLiteral(
                        "DXR instance field must be an integer in [0,%1]")
                        .arg(maximum));
                return false;
            }
            out = static_cast<UINT>(value);
            return true;
        };
        auto customIndex = static_cast<UINT>(instanceIndex);
        auto mask = UINT{ };
        auto hitGroupOffset = UINT{ };
        if ((!instance.customIndex.isEmpty()
                && !readField(instance.customIndex, 0xffffff, customIndex))
            || customIndex > 0xffffff || !readField(instance.mask, 0xff, mask)
            || !readField(instance.hitGroupOffset, 0xffffff, hitGroupOffset))
            return false;

        auto instanceDesc = D3D12_RAYTRACING_INSTANCE_DESC{ };
        instanceDesc.Transform[0][0] = 1.0f;
        instanceDesc.Transform[1][1] = 1.0f;
        instanceDesc.Transform[2][2] = 1.0f;
        if (!instance.transform.isEmpty()) {
            auto transform = scriptEngine.evaluateValues(instance.transform,
                instance.itemId);
            if (transform.size() != 12 && transform.size() != 16) {
                mMessages.insert(instance.itemId,
                    MessageType::UniformComponentMismatch,
                    QStringLiteral("(%1/12 or 16)").arg(transform.size()));
                return false;
            }
            transform.resize(12);
            for (auto i = 0; i < transform.size(); ++i)
                instanceDesc.Transform[i / 4][i % 4] = transform[i];
        }
        instanceDesc.InstanceID = customIndex;
        instanceDesc.InstanceMask = mask;
        instanceDesc.InstanceContributionToHitGroupIndex = hitGroupOffset;
        instanceDesc.Flags =
            D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
        instanceDesc.AccelerationStructure =
            bottomLevel->GetGPUVirtualAddress();
        instanceDescs.push_back(instanceDesc);
    }

    if (instanceDescs.empty())
        return false;

    const auto instanceBufferSize = instanceDescs.size()
        * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
    if (mTopLevel && mLastInstanceDescs.size() == instanceDescs.size()
        && std::memcmp(mLastInstanceDescs.data(), instanceDescs.data(),
               instanceBufferSize)
            == 0) {
        return true;
    }

    // retain old storage for dispatches already recorded in this list
    auto instanceBuffer = createBuffer(context.device, instanceBufferSize,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!instanceBuffer)
        return false;
    auto mappedData = std::add_pointer_t<void>{ };
    if (FAILED(instanceBuffer->Map(0, nullptr, &mappedData)))
        return false;
    std::memcpy(mappedData, instanceDescs.data(), instanceBufferSize);
    instanceBuffer->Unmap(0, nullptr);

    const auto update = mAllowTlasUpdate && mTopLevel
        && mLastInstanceDescs.size() == instanceDescs.size();
    auto inputs = D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS{
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags =
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
        .NumDescs = static_cast<UINT>(instanceDescs.size()),
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .InstanceDescs = instanceBuffer->GetGPUVirtualAddress(),
    };
    if (mAllowTlasUpdate)
        inputs.Flags |=
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    auto info = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO{ };
    if (update) {
        inputs.Flags |=
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
        info.ResultDataMaxSizeInBytes = mTopLevelResultSize;
        info.ScratchDataSizeInBytes = mTopLevelUpdateScratchSize;
    } else {
        device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
    }
    const auto sourceAddress = update ? gpuAddress()
                                      : D3D12_GPU_VIRTUAL_ADDRESS{ };
    auto topLevel = createBuffer(*device.Get(), info.ResultDataMaxSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto scratch = createBuffer(*device.Get(), info.ScratchDataSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    if (!topLevel || !scratch)
        return false;
    const auto buildDesc = D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC{
        .DestAccelerationStructureData = topLevel->GetGPUVirtualAddress(),
        .Inputs = inputs,
        .SourceAccelerationStructureData = sourceAddress,
        .ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress(),
    };
    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(topLevel.Get());
    commandList->ResourceBarrier(1, &barrier);
    if (mTopLevel)
        context.stagingBuffers.push_back(std::move(mTopLevel));
    if (mInstanceBuffer)
        context.stagingBuffers.push_back(std::move(mInstanceBuffer));
    context.stagingBuffers.push_back(std::move(scratch));
    mTopLevel = std::move(topLevel);
    mInstanceBuffer = std::move(instanceBuffer);
    if (!update) {
        mTopLevelResultSize = info.ResultDataMaxSizeInBytes;
        mTopLevelUpdateScratchSize = info.UpdateScratchDataSizeInBytes;
    }
    mLastInstanceDescs = std::move(instanceDescs);
    return true;
}
