#include "D3DPipeline.h"
#include "D3DAccelerationStructure.h"
#include "D3DProgram.h"
#include <set>

bool D3DPipeline::createRayTracing(D3DContext &context,
    D3DAccelerationStructure *accelerationStructure)
{
    if (std::exchange(mCreated, true))
        return (mRayTracingStateObject && mRootSignature);

    mAccelerationStructure = accelerationStructure;
    if (!createRootSignatureRayTracing(context))
        return false;
    createDescriptorHeap(context);

    auto device = ComPtr<ID3D12Device5>();
    if (FAILED(context.device.QueryInterface(IID_PPV_ARGS(&device)))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    const auto rayGeneration =
        mProgram.getShader(Shader::ShaderType::RayGeneration);
    const auto rayMiss = mProgram.getShader(Shader::ShaderType::RayMiss);
    const auto closestHit =
        mProgram.getShader(Shader::ShaderType::RayClosestHit);
    const auto anyHit = mProgram.getShader(Shader::ShaderType::RayAnyHit);
    const auto intersection =
        mProgram.getShader(Shader::ShaderType::RayIntersection);
    if (!rayGeneration || !rayMiss) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    auto stateObject =
        CD3DX12_STATE_OBJECT_DESC(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);
    for (const auto &shader : mProgram.shaders()) {
        if (!isRayTracingShaderType(shader.type()))
            continue;

        const auto bytecode = D3D12_SHADER_BYTECODE{
            shader.binary()->GetBufferPointer(),
            shader.binary()->GetBufferSize(),
        };
        auto library =
            stateObject.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        library->SetDXILLibrary(&bytecode);
        library->DefineExport(shader.entryPoint().toStdWString().c_str());
    }

    constexpr auto hitGroupName = L"HitGroup";
    if (closestHit || anyHit || intersection) {
        auto hitGroup =
            stateObject.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetHitGroupExport(hitGroupName);
        hitGroup->SetHitGroupType(intersection
                ? D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE
                : D3D12_HIT_GROUP_TYPE_TRIANGLES);
        if (closestHit)
            hitGroup->SetClosestHitShaderImport(
                closestHit->entryPoint().toStdWString().c_str());
        if (anyHit)
            hitGroup->SetAnyHitShaderImport(
                anyHit->entryPoint().toStdWString().c_str());
        if (intersection)
            hitGroup->SetIntersectionShaderImport(
                intersection->entryPoint().toStdWString().c_str());
    }

    auto shaderConfig =
        stateObject
            .CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    shaderConfig->Config(128, D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES);

    auto rootSignature =
        stateObject.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    rootSignature->SetRootSignature(mRootSignature.Get());

    auto pipelineConfig =
        stateObject
            .CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pipelineConfig->Config(1);

    if (FAILED(device->CreateStateObject(stateObject,
            IID_PPV_ARGS(&mRayTracingStateObject)))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    auto properties = ComPtr<ID3D12StateObjectProperties>();
    if (FAILED(mRayTracingStateObject.As(&properties))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    constexpr auto identifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    constexpr auto tableAlignment =
        D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
    constexpr auto rayGenerationOffset = UINT64{ 0 };
    constexpr auto missOffset = UINT64{ tableAlignment };
    constexpr auto hitGroupOffset = UINT64{ tableAlignment * 2 };
    constexpr auto bufferSize = UINT64{ tableAlignment * 3 };

    const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    if (FAILED(context.device.CreateCommittedResource(&heapProperties,
            D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&mShaderBindingTable)))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    auto mappedData = std::add_pointer_t<void>{ };
    if (FAILED(mShaderBindingTable->Map(0, nullptr, &mappedData))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }
    std::memset(mappedData, 0, static_cast<size_t>(bufferSize));
    const auto copyIdentifier = [&](UINT64 offset, const wchar_t *name) {
        if (const auto identifier = properties->GetShaderIdentifier(name)) {
            std::memcpy(static_cast<std::byte *>(mappedData) + offset,
                identifier, identifierSize);
            return true;
        }
        return false;
    };
    const auto identifiersValid =
        copyIdentifier(rayGenerationOffset,
            rayGeneration->entryPoint().toStdWString().c_str())
        && copyIdentifier(missOffset,
            rayMiss->entryPoint().toStdWString().c_str())
        && (!(closestHit || anyHit || intersection)
            || copyIdentifier(hitGroupOffset, hitGroupName));
    mShaderBindingTable->Unmap(0, nullptr);
    if (!identifiersValid) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    const auto address = mShaderBindingTable->GetGPUVirtualAddress();
    mRayGenerationShaderRecord = {
        .StartAddress = address + rayGenerationOffset,
        .SizeInBytes = identifierSize,
    };
    mMissShaderTable = {
        .StartAddress = address + missOffset,
        .SizeInBytes = identifierSize,
        .StrideInBytes = identifierSize,
    };
    if (closestHit || anyHit || intersection)
        mHitGroupTable = {
            .StartAddress = address + hitGroupOffset,
            .SizeInBytes = identifierSize,
            .StrideInBytes = identifierSize,
        };
    return true;
}

bool D3DPipeline::createRootSignatureRayTracing(D3DContext &context)
{
    auto descriptorTableDescs = std::vector<DescriptorTableDesc>();
    auto staticSamplers = std::vector<CD3DX12_STATIC_SAMPLER_DESC>();
    auto ranges = std::vector<CD3DX12_DESCRIPTOR_RANGE>();
    auto resources =
        std::set<std::tuple<D3D12_DESCRIPTOR_RANGE_TYPE, UINT, UINT>>();
    auto samplers = std::set<std::pair<UINT, UINT>>();
    auto descriptorHeapOffset = UINT{ };
    for (const auto &shader : mProgram.shaders()) {
        if (!shader.reflection())
            continue;
        for (const auto &binding : shader.reflection().descriptorBindings()) {
            const auto bindDesc = getD3DBindingDesc(binding);
            if (bindDesc.Type == D3D_SIT_SAMPLER) {
                if (samplers.emplace(bindDesc.Space, bindDesc.BindPoint).second)
                    addStaticSampler(bindDesc, &staticSamplers);
                continue;
            }

            // TODO: fix non uniform indexing
            if (!bindDesc.BindCount) {
                mMessages.insert(mItemId, MessageType::NotImplemented,
                    "Non-Uniform Indexing");
                return false;
            }

            const auto rangeType = getRangeType(bindDesc.Type);
            if (!resources
                    .emplace(rangeType, bindDesc.Space, bindDesc.BindPoint)
                    .second)
                continue;
            ranges.emplace_back().Init(rangeType, bindDesc.BindCount,
                bindDesc.BindPoint, bindDesc.Space);
            mDescriptorHeapEntries.push_back({
                .visibility = D3D12_SHADER_VISIBILITY_ALL,
                .rangeType = rangeType,
                .space = bindDesc.Space,
                .bindPoint = bindDesc.BindPoint,
                .offset = descriptorHeapOffset,
                .count = bindDesc.BindCount,
            });
            descriptorHeapOffset += bindDesc.BindCount;
        }
    }

    if (!ranges.empty()) {
        mDescriptorTableEntries.push_back(descriptorHeapOffset);
        descriptorTableDescs.push_back({
            .visibility = D3D12_SHADER_VISIBILITY_ALL,
            .ranges = std::move(ranges),
        });
    }
    return createRootSignatureFromTables(context,
        std::move(descriptorTableDescs), std::move(staticSamplers));
}

bool D3DPipeline::bindRayTracing(D3DContext &context,
    ScriptEngine &scriptEngine)
{
    auto commandList = ComPtr<ID3D12GraphicsCommandList4>();
    if (FAILED(context.graphicsCommandList.As(&commandList))) {
        mMessages.insert(mItemId, MessageType::CallFailed);
        return false;
    }

    commandList->SetPipelineState1(mRayTracingStateObject.Get());
    commandList->SetComputeRootSignature(mRootSignature.Get());

    if (!setDescriptors(context, scriptEngine))
        return false;

    if (mDescriptorHeap) {
        auto descriptorHeap = mDescriptorHeap.Get();
        commandList->SetDescriptorHeaps(1, &descriptorHeap);

        auto descriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE{
            mDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
        };
        for (auto i = 0u; i < mDescriptorTableEntries.size(); ++i) {
            commandList->SetComputeRootDescriptorTable(i, descriptor);
            descriptor.Offset(mDescriptorTableEntries[i],
                context.descriptorSize);
        }
    }
    return true;
}

void D3DPipeline::dispatchRays(D3DContext &context, UINT width, UINT height,
    UINT depth)
{
    auto commandList = ComPtr<ID3D12GraphicsCommandList4>();
    if (FAILED(context.graphicsCommandList.As(&commandList))) {
        mMessages.insert(mItemId, MessageType::CallFailed);
        return;
    }

    const auto desc = D3D12_DISPATCH_RAYS_DESC{
        .RayGenerationShaderRecord = mRayGenerationShaderRecord,
        .MissShaderTable = mMissShaderTable,
        .HitGroupTable = mHitGroupTable,
        .Width = width,
        .Height = height,
        .Depth = depth,
    };
    commandList->DispatchRays(&desc);
}
