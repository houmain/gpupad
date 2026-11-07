#include "D3DAccelerationStructure.h"

D3DAccelerationStructure::D3DAccelerationStructure(
    const AccelerationStructure &accelStruct)
    : mItemId(accelStruct.id)
{
    mUsedItems += accelStruct.id;
    for (auto item : accelStruct.items)
        if (auto instance = castItem<Instance>(item))
            mInstances.push_back({
                .itemId = instance->id,
                .geometrySetId = (instance->items.empty()
                        ? instance->geometrySetId
                        : instance->id),
                .transform = instance->transform,
                .shaderInstanceId = instance->shaderInstanceId,
                .visibilityMask = instance->visibilityMask,
                .hitGroupRecordOffset = instance->hitGroupRecordOffset,
                .triangleCulling = instance->triangleCulling,
                .frontFace = instance->frontFace,
                .opacityMode = instance->opacityMode,
            });
}

bool D3DAccelerationStructure::operator==(
    const D3DAccelerationStructure &rhs) const
{
    if (mInstances != rhs.mInstances)
        return false;
    for (auto i = 0u; i < mInstances.size(); ++i) {
        const auto lhsSet = mInstances[i].geometrySet;
        const auto rhsSet = rhs.mInstances[i].geometrySet;
        if (static_cast<bool>(lhsSet) != static_cast<bool>(rhsSet)
            || (lhsSet && *lhsSet != *rhsSet))
            return false;
    }
    return true;
}

void D3DAccelerationStructure::setGeometrySet(
    int instanceIndex, D3DGeometrySet *geometrySet)
{
    Q_ASSERT(instanceIndex >= 0
        && instanceIndex < static_cast<int>(mInstances.size()));
    mInstances[instanceIndex].geometrySet = geometrySet;
}

bool D3DAccelerationStructure::build(D3DContext &context,
    ScriptEngine &scriptEngine)
{
    auto device = ComPtr<ID3D12Device5>();
    auto commandList = ComPtr<ID3D12GraphicsCommandList4>();
    if (FAILED(context.device.QueryInterface(IID_PPV_ARGS(&device)))
        || FAILED(context.graphicsCommandList.As(&commandList)))
        return false;

    auto instanceDescs = std::vector<D3D12_RAYTRACING_INSTANCE_DESC>();
    auto geometryRevisions = std::vector<uint64_t>();
    for (auto instanceIndex = 0u; instanceIndex < mInstances.size();
         ++instanceIndex) {
        const auto &instance = mInstances[instanceIndex];
        mUsedItems += instance.itemId;
        if (!instance.geometrySet
            || !instance.geometrySet->build(context, scriptEngine))
            return false;
        mUsedItems += instance.geometrySet->usedItems();

        auto shaderInstanceId = static_cast<uint32_t>(instanceIndex);
        if (!instance.shaderInstanceId.isEmpty()) {
            const auto value = scriptEngine.evaluateUInt(
                instance.shaderInstanceId, instance.itemId, 0, 0xFFFFFF,
                QStringLiteral("Shader instance ID"));
            if (!value)
                return false;
            shaderInstanceId = *value;
        }
        const auto visibilityMask = scriptEngine.evaluateUInt(
            instance.visibilityMask, instance.itemId, 0, 0xFF,
            QStringLiteral("Visibility mask"));
        if (!visibilityMask)
            return false;
        const auto hitGroupRecordOffset = scriptEngine.evaluateUInt(
            instance.hitGroupRecordOffset, instance.itemId, 0, 0xFFFFFF,
            QStringLiteral("Hit group record offset"));
        if (!hitGroupRecordOffset)
            return false;

        auto instanceDesc = D3D12_RAYTRACING_INSTANCE_DESC{ };
        instanceDesc.Transform[0][0] = 1.0f;
        instanceDesc.Transform[1][1] = 1.0f;
        instanceDesc.Transform[2][2] = 1.0f;
        if (!instance.transform.isEmpty()) {
            auto transform = scriptEngine.evaluateValues(
                instance.transform, instance.itemId);
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
        instanceDesc.InstanceID = shaderInstanceId;
        instanceDesc.InstanceMask = *visibilityMask;
        instanceDesc.InstanceContributionToHitGroupIndex =
            *hitGroupRecordOffset;
        instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        if (!instance.triangleCulling)
            instanceDesc.Flags |=
                D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
        if (instance.frontFace == Instance::FrontFace::CCW)
            instanceDesc.Flags |=
                D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
        if (instance.opacityMode == Instance::OpacityMode::ForceOpaque)
            instanceDesc.Flags |=
                D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
        else if (instance.opacityMode
            == Instance::OpacityMode::ForceNonOpaque)
            instanceDesc.Flags |=
                D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;
        instanceDesc.AccelerationStructure =
            instance.geometrySet->gpuAddress();
        instanceDescs.push_back(instanceDesc);
        geometryRevisions.push_back(instance.geometrySet->revision());
    }

    if (instanceDescs.empty())
        return false;

    if (mTopLevel && mLastInstanceDescs.size() == instanceDescs.size()
        && mLastGeometryRevisions == geometryRevisions
        && std::memcmp(mLastInstanceDescs.data(), instanceDescs.data(),
               instanceDescs.size()
                   * sizeof(D3D12_RAYTRACING_INSTANCE_DESC))
            == 0)
        return true;

    const auto instanceBufferSize =
        instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
    auto instanceBuffer = createBuffer(context.device, instanceBufferSize,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!instanceBuffer)
        return false;
    auto mappedData = std::add_pointer_t<void>{ };
    if (FAILED(instanceBuffer->Map(0, nullptr, &mappedData)))
        return false;
    std::memcpy(mappedData, instanceDescs.data(), instanceBufferSize);
    instanceBuffer->Unmap(0, nullptr);

    const auto update = static_cast<bool>(mTopLevel);
    auto inputs = D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS{
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags =
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE
            | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE,
        .NumDescs = static_cast<UINT>(instanceDescs.size()),
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .InstanceDescs = instanceBuffer->GetGPUVirtualAddress(),
    };
    if (update)
        inputs.Flags |=
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;

    auto info = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO{ };
    if (update) {
        info.ScratchDataSizeInBytes = mUpdateScratchSize;
    } else {
        device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
    }

    auto topLevel = update
        ? mTopLevel
        : createBuffer(*device.Get(), info.ResultDataMaxSizeInBytes,
              D3D12_HEAP_TYPE_DEFAULT,
              D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
              D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto scratchBuffer = createBuffer(*device.Get(),
        info.ScratchDataSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    if (!topLevel || !scratchBuffer)
        return false;

    if (update) {
        const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(topLevel.Get());
        commandList->ResourceBarrier(1, &barrier);
    }

    const auto buildDesc =
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC{
            .DestAccelerationStructureData =
                topLevel->GetGPUVirtualAddress(),
            .Inputs = inputs,
            .SourceAccelerationStructureData = update
                ? mTopLevel->GetGPUVirtualAddress()
                : D3D12_GPU_VIRTUAL_ADDRESS{},
            .ScratchAccelerationStructureData =
                scratchBuffer->GetGPUVirtualAddress(),
        };
    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(topLevel.Get());
    commandList->ResourceBarrier(1, &barrier);

    if (mInstanceBuffer)
        context.stagingBuffers.push_back(std::move(mInstanceBuffer));
    context.stagingBuffers.push_back(std::move(scratchBuffer));
    mInstanceBuffer = std::move(instanceBuffer);
    if (!update) {
        mTopLevel = std::move(topLevel);
        mUpdateScratchSize = info.UpdateScratchDataSizeInBytes;
    }
    mLastInstanceDescs = std::move(instanceDescs);
    mLastGeometryRevisions = std::move(geometryRevisions);
    return true;
}
