#include "D3DAccelerationStructure.h"

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
    if (mTopLevel)
        return true;

    auto device = ComPtr<ID3D12Device5>();
    auto commandList = ComPtr<ID3D12GraphicsCommandList4>();
    if (FAILED(context.device.QueryInterface(IID_PPV_ARGS(&device)))
        || FAILED(context.graphicsCommandList.As(&commandList)))
        return false;

    auto instanceDescs = std::vector<D3D12_RAYTRACING_INSTANCE_DESC>();
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
    }

    if (instanceDescs.empty())
        return false;

    const auto instanceBufferSize =
        instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
    mInstanceBuffer = createBuffer(context.device, instanceBufferSize,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!mInstanceBuffer)
        return false;
    auto mappedData = std::add_pointer_t<void>{ };
    if (FAILED(mInstanceBuffer->Map(0, nullptr, &mappedData)))
        return false;
    std::memcpy(mappedData, instanceDescs.data(), instanceBufferSize);
    mInstanceBuffer->Unmap(0, nullptr);

    const auto inputs = D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS{
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags =
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
        .NumDescs = static_cast<UINT>(instanceDescs.size()),
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .InstanceDescs = mInstanceBuffer->GetGPUVirtualAddress(),
    };
    auto info = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO{ };
    device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
    mTopLevel = createBuffer(*device.Get(), info.ResultDataMaxSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    mScratchBuffer = createBuffer(*device.Get(), info.ScratchDataSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    if (!mTopLevel || !mScratchBuffer) {
        mTopLevel.Reset();
        mScratchBuffer.Reset();
        return false;
    }

    const auto buildDesc =
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC{
            .DestAccelerationStructureData =
                mTopLevel->GetGPUVirtualAddress(),
            .Inputs = inputs,
            .ScratchAccelerationStructureData =
                mScratchBuffer->GetGPUVirtualAddress(),
        };
    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(mTopLevel.Get());
    commandList->ResourceBarrier(1, &barrier);
    return true;
}
