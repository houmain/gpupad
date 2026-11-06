#include "D3DPipeline.h"
#include "D3DAccelerationStructure.h"
#include "D3DProgram.h"
#include <set>

bool D3DPipeline::hasSameLocalBindingLayout(
    const std::vector<LocalBinding> &left,
    const std::vector<LocalBinding> &right)
{
    return left.size() == right.size()
        && std::equal(left.begin(), left.end(), right.begin(),
            [](const auto &a, const auto &b) {
                return a.hasSameLayout(b);
            });
}

bool D3DPipeline::createRayTracing(D3DContext &context)
{
    if (std::exchange(mCreated, true))
        return (mRayTracingStateObject && mRootSignature);

    auto device = ComPtr<ID3D12Device5>();
    if (FAILED(context.device.QueryInterface(IID_PPV_ARGS(&device)))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    if (!mShaderTable) {
        mMessages.insert(mItemId, MessageType::ScriptError,
            "No shader table set");
        return false;
    }
    mUsedItems += mShaderTable->id;

    const auto fail = [&](ItemId itemId, const QString &text) {
        mMessages.insert(itemId, MessageType::ScriptError, text);
        return false;
    };
    const auto getShader = [&](ItemId shaderId, Shader::ShaderType type,
                               const QString &role) -> const D3DShader * {
        const auto shader = mProgram.getShader(shaderId);
        if (!shader || shader->type() != type) {
            fail(shaderId ? shaderId : mShaderTable->id,
                QStringLiteral("%1 has an invalid shader reference")
                    .arg(role));
            return nullptr;
        }
        return shader;
    };

    const Record *rayGenerationRecord = nullptr;
    auto missRecords = std::vector<const Record *>();
    auto hitRecords = std::vector<const Record *>();
    for (const auto item : mShaderTable->items) {
        const auto record = castItem<Record>(item);
        if (!record)
            continue;
        mUsedItems += record->id;
        switch (record->recordType) {
        case Record::RecordType::RayGeneration:
            if (rayGenerationRecord)
                return fail(record->id,
                    "A shader table must contain exactly one ray-generation record");
            rayGenerationRecord = record;
            break;
        case Record::RecordType::Miss: missRecords.push_back(record); break;
        case Record::RecordType::Hit:  hitRecords.push_back(record); break;
        }
    }
    if (!rayGenerationRecord)
        return fail(mShaderTable->id,
            "A shader table must contain exactly one ray-generation record");

    const auto rayGeneration = getShader(rayGenerationRecord->shaderId,
        Shader::ShaderType::RayGeneration, "Ray-generation record");
    if (!rayGeneration)
        return false;
    for (const auto record : missRecords)
        if (!getShader(record->shaderId, Shader::ShaderType::RayMiss,
                "Miss record"))
            return false;
    for (const auto record : hitRecords) {
        if (record->intersectionShaderId
            && !getShader(record->intersectionShaderId,
                Shader::ShaderType::RayIntersection,
                "Intersection shader"))
            return false;
        if (record->anyHitShaderId
            && !getShader(record->anyHitShaderId,
                Shader::ShaderType::RayAnyHit, "Any-hit shader"))
            return false;
        if (record->closestHitShaderId
            && !getShader(record->closestHitShaderId,
                Shader::ShaderType::RayClosestHit,
                "Closest-hit shader"))
            return false;
    }

    const auto shaderExport = [](ItemId itemId) {
        return QStringLiteral("Shader_%1").arg(itemId).toStdWString();
    };
    const auto hitGroupExport = [](ItemId itemId) {
        return QStringLiteral("HitGroup_%1").arg(itemId).toStdWString();
    };

    mRayGenerationRecordData = {
        .item = rayGenerationRecord,
        .exportName = shaderExport(rayGenerationRecord->shaderId),
    };
    if (!prepareLocalBindings(mRayGenerationRecordData, { rayGeneration }))
        return false;
    for (const auto record : missRecords) {
        const auto shader = mProgram.getShader(record->shaderId);
        auto &data = mMissRecordData.emplace_back(RayTracingRecord{
            .item = record,
            .exportName = shaderExport(record->shaderId),
        });
        if (!prepareLocalBindings(data, { shader }))
            return false;
    }
    for (const auto record : hitRecords) {
        const auto intersection =
            mProgram.getShader(record->intersectionShaderId);
        const auto anyHit = mProgram.getShader(record->anyHitShaderId);
        const auto closestHit =
            mProgram.getShader(record->closestHitShaderId);
        auto &data = mHitRecordData.emplace_back(RayTracingRecord{
            .item = record,
            .exportName = hitGroupExport(record->id),
        });
        if (!prepareLocalBindings(data,
                { intersection, anyHit, closestHit }))
            return false;
    }

    auto shaderLayouts = std::map<ItemId,
        const std::vector<LocalBinding> *>();
    for (const auto &data : mHitRecordData)
        for (const auto shaderId : { data.item->intersectionShaderId,
                 data.item->anyHitShaderId,
                 data.item->closestHitShaderId }) {
            if (!shaderId)
                continue;
            const auto [it, inserted] =
                shaderLayouts.emplace(shaderId, &data.localBindings);
            if (!inserted
                && !hasSameLocalBindingLayout(
                    *it->second, data.localBindings))
                return fail(data.item->id,
                    "Hit records sharing a shader must use the same local binding layout");
        }

    if (!createRootSignatureRayTracing(context))
        return false;
    createDescriptorHeap(context);

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
        const auto exportName =
            QStringLiteral("Shader_%1").arg(shader.itemId()).toStdWString();
        library->DefineExport(exportName.c_str(),
            shader.entryPoint().toStdWString().c_str());
    }

    for (const auto record : hitRecords) {
        auto hitGroup =
            stateObject.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        const auto groupName = hitGroupExport(record->id);
        hitGroup->SetHitGroupExport(groupName.c_str());
        hitGroup->SetHitGroupType(record->intersectionShaderId
                ? D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE
                : D3D12_HIT_GROUP_TYPE_TRIANGLES);
        if (record->closestHitShaderId) {
            const auto name = shaderExport(record->closestHitShaderId);
            hitGroup->SetClosestHitShaderImport(name.c_str());
        }
        if (record->anyHitShaderId) {
            const auto name = shaderExport(record->anyHitShaderId);
            hitGroup->SetAnyHitShaderImport(name.c_str());
        }
        if (record->intersectionShaderId) {
            const auto name = shaderExport(record->intersectionShaderId);
            hitGroup->SetIntersectionShaderImport(name.c_str());
        }
    }

    auto shaderConfig =
        stateObject
            .CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    shaderConfig->Config(128, D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES);

    auto rootSignature =
        stateObject.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    rootSignature->SetRootSignature(mRootSignature.Get());

    if (!createLocalRootSignatures(context, stateObject))
        return false;

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

    constexpr auto identifierSize =
        UINT64{ D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES };
    constexpr auto tableAlignment =
        UINT64{ D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT };
    constexpr auto rayGenerationOffset = UINT64{ 0 };
    const auto alignTable = [](UINT64 value) {
        constexpr auto alignment =
            UINT64{ D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT };
        return (value + alignment - 1) & ~(alignment - 1);
    };
    const auto recordSize = [&](const RayTracingRecord &record) {
        auto size = identifierSize;
        for (const auto &binding : record.localBindings)
            size += binding.count() * sizeof(uint32_t);
        constexpr auto alignment =
            UINT64{ D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT };
        return (size + alignment - 1) & ~(alignment - 1);
    };
    auto rayGenerationSize = recordSize(mRayGenerationRecordData);
    auto missStride = identifierSize;
    for (const auto &record : mMissRecordData)
        missStride = std::max(missStride, recordSize(record));
    auto hitStride = identifierSize;
    for (const auto &record : mHitRecordData)
        hitStride = std::max(hitStride, recordSize(record));

    const auto missOffset = alignTable(rayGenerationSize);
    const auto hitGroupOffset =
        alignTable(missOffset + missRecords.size() * missStride);
    const auto bufferSize = alignTable(
        hitGroupOffset + hitRecords.size() * hitStride);

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
    mRayGenerationRecordData.offset = rayGenerationOffset;
    auto identifiersValid = copyIdentifier(rayGenerationOffset,
        mRayGenerationRecordData.exportName.c_str());
    for (auto i = size_t{ }; i < mMissRecordData.size(); ++i) {
        mMissRecordData[i].offset = missOffset + i * missStride;
        identifiersValid &= copyIdentifier(
            mMissRecordData[i].offset,
            mMissRecordData[i].exportName.c_str());
    }
    for (auto i = size_t{ }; i < mHitRecordData.size(); ++i) {
        mHitRecordData[i].offset = hitGroupOffset + i * hitStride;
        identifiersValid &= copyIdentifier(
            mHitRecordData[i].offset,
            mHitRecordData[i].exportName.c_str());
    }
    mShaderBindingTable->Unmap(0, nullptr);
    if (!identifiersValid) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    const auto address = mShaderBindingTable->GetGPUVirtualAddress();
    mRayGenerationShaderRecord = {
        .StartAddress = address + rayGenerationOffset,
        .SizeInBytes = rayGenerationSize,
    };
    if (!missRecords.empty())
        mMissShaderTable = {
            .StartAddress = address + missOffset,
            .SizeInBytes = missRecords.size() * missStride,
            .StrideInBytes = missStride,
        };
    if (!hitRecords.empty())
        mHitGroupTable = {
            .StartAddress = address + hitGroupOffset,
            .SizeInBytes = hitRecords.size() * hitStride,
            .StrideInBytes = hitStride,
        };
    return true;
}

bool D3DPipeline::prepareLocalBindings(RayTracingRecord &record,
    std::initializer_list<const D3DShader *> shaders)
{
    for (const auto item : record.item->items) {
        const auto binding = castItem<Binding>(item);
        if (!binding)
            continue;
        mUsedItems += binding->id;
        if (binding->bindingType != Binding::BindingType::Uniform) {
            mMessages.insert(binding->id, MessageType::ScriptError,
                "Shader-table records currently support only Uniform bindings");
            return false;
        }

        auto local = LocalBinding{ .binding = binding };
        auto found = false;
        for (const auto shader : shaders) {
            if (!shader || !shader->reflection())
                continue;
            for (const auto &desc : shader->reflection().descriptorBindings()) {
                const auto bindDesc = getD3DBindingDesc(desc);
                if (bindDesc.Type != D3D_SIT_CBUFFER)
                    continue;
                const auto descriptorName =
                    QString::fromUtf8(desc.name ? desc.name : "");
                const auto typeName = QString::fromUtf8(desc.type_description
                        && desc.type_description->type_name
                    ? desc.type_description->type_name
                    : "");
                if (binding->name != descriptorName && binding->name != typeName)
                    continue;

                const auto candidate = LocalBinding{
                    .binding = binding,
                    .shaderRegister = bindDesc.BindPoint,
                    .registerSpace = bindDesc.Space,
                    .layout = getShaderRecordLayout(desc.block),
                };
                if (!candidate.count()) {
                    mMessages.insert(binding->id, MessageType::ScriptError,
                        "Local constant buffer has no reflected data");
                    return false;
                }
                if (found && !local.hasSameLayout(candidate)) {
                    mMessages.insert(binding->id, MessageType::ScriptError,
                        "Local binding resolves to incompatible constant buffers");
                    return false;
                }
                local = candidate;
                found = true;
            }
        }
        if (!found) {
            mMessages.insert(binding->id, MessageType::ScriptError,
                QStringLiteral("No reflected constant buffer named '%1'")
                    .arg(binding->name));
            return false;
        }
        for (const auto &existing : record.localBindings)
            if (std::tie(existing.registerSpace, existing.shaderRegister)
                == std::tie(local.registerSpace, local.shaderRegister)) {
                mMessages.insert(binding->id, MessageType::ScriptError,
                    "Duplicate local constant-buffer binding");
                return false;
            }
        record.localBindings.push_back(local);
        mLocalConstantRegisters.emplace(
            local.registerSpace, local.shaderRegister);
    }
    return true;
}

bool D3DPipeline::createLocalRootSignatures(D3DContext &context,
    CD3DX12_STATE_OBJECT_DESC &stateObject)
{
    auto recordsByExport = std::map<std::wstring, const RayTracingRecord *>();
    const auto add = [&](const RayTracingRecord &record) {
        if (record.localBindings.empty())
            return true;
        const auto [it, inserted] =
            recordsByExport.emplace(record.exportName, &record);
        if (inserted)
            return true;
        return hasSameLocalBindingLayout(
            it->second->localBindings, record.localBindings);
    };
    if (!add(mRayGenerationRecordData)) {
        mMessages.insert(mShaderTable->id, MessageType::ScriptError,
            "Records sharing a shader must use the same local binding layout");
        return false;
    }
    for (const auto &record : mMissRecordData)
        if (!add(record)) {
            mMessages.insert(record.item->id, MessageType::ScriptError,
                "Records sharing a shader must use the same local binding layout");
            return false;
        }
    for (const auto &record : mHitRecordData)
        if (!add(record))
            return false;

    for (const auto &[exportName, record] : recordsByExport) {
        auto parameters = std::vector<CD3DX12_ROOT_PARAMETER>();
        parameters.reserve(record->localBindings.size());
        for (const auto &binding : record->localBindings)
            parameters.emplace_back().InitAsConstants(binding.count(),
                binding.shaderRegister, binding.registerSpace);
        const auto desc = CD3DX12_ROOT_SIGNATURE_DESC(
            static_cast<UINT>(parameters.size()), parameters.data(), 0,
            nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
        auto blob = ComPtr<ID3DBlob>();
        auto errors = ComPtr<ID3DBlob>();
        if (FAILED(D3D12SerializeRootSignature(&desc,
                D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors))) {
            mMessages.insert(record->item->id, MessageType::ScriptError,
                errors ? QString::fromUtf8(static_cast<const char *>(
                             errors->GetBufferPointer()),
                             static_cast<qsizetype>(errors->GetBufferSize()))
                       : "Cannot serialize local root signature");
            return false;
        }
        auto signature = ComPtr<ID3D12RootSignature>();
        if (FAILED(context.device.CreateRootSignature(0,
                blob->GetBufferPointer(), blob->GetBufferSize(),
                IID_PPV_ARGS(&signature)))) {
            mMessages.insert(record->item->id, MessageType::ScriptError,
                "Cannot create local root signature");
            return false;
        }
        auto local = stateObject.CreateSubobject<
            CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
        local->SetRootSignature(signature.Get());
        auto association = stateObject.CreateSubobject<
            CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
        association->SetSubobjectToAssociate(*local);
        association->AddExport(exportName.c_str());
        mLocalRootSignatures.push_back(std::move(signature));
    }
    return true;
}

bool D3DPipeline::updateShaderTable(ScriptEngine &scriptEngine)
{
    auto mappedData = std::add_pointer_t<void>{};
    if (FAILED(mShaderBindingTable->Map(0, nullptr, &mappedData))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }
    const auto write = [&](const RayTracingRecord &record) {
        auto destination = static_cast<std::byte *>(mappedData)
            + record.offset + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        for (const auto &binding : record.localBindings) {
            const auto values = mBindings.evaluatedUniformValues.value(
                binding.binding->id);
            checkValueCount(values.size(), -1,
                static_cast<int>(binding.layout.elements.size()),
                binding.binding->id, scriptEngine.messages());

            auto data = packShaderRecordData(binding.layout, values);
            data.resize(binding.count() * sizeof(uint32_t));
            std::memcpy(destination, data.data(), data.size());
            destination += data.size();
        }
    };
    write(mRayGenerationRecordData);
    for (const auto &record : mMissRecordData)
        write(record);
    for (const auto &record : mHitRecordData)
        write(record);
    mShaderBindingTable->Unmap(0, nullptr);
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
            if (bindDesc.Type == D3D_SIT_CBUFFER
                && mLocalConstantRegisters.contains(
                    { bindDesc.Space, bindDesc.BindPoint }))
                continue;
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

    if (!updateShaderTable(scriptEngine)
        || !setDescriptors(context, scriptEngine))
        return false;

    commandList->SetPipelineState1(mRayTracingStateObject.Get());
    commandList->SetComputeRootSignature(mRootSignature.Get());

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
