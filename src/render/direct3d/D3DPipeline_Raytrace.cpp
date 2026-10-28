#include "D3DPipeline.h"
#include "D3DAccelerationStructure.h"
#include "D3DProgram.h"
#include <set>

namespace {
    bool readDword(const QVariant &value, UINT &result)
    {
        switch (value.typeId()) {
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Double:
        case QMetaType::Float:     break;
        default:                   return false;
        }
        const auto number = value.toDouble();
        if (!(number >= 0.0 && number <= 4294967295.0))
            return false;
        result = static_cast<UINT>(number);
        return number == result;
    }
} // namespace

bool D3DPipeline::createRayTracing(D3DContext &context,
    D3DAccelerationStructure *accelerationStructure)
{
    if (std::exchange(mCreated, true))
        return (mRayTracingStateObject && mRootSignature);

    mAccelerationStructure = accelerationStructure;
    if (!prepareLocalRoots())
        return false;
    if (!createRootSignatureRayTracing(context))
        return false;
    createDescriptorHeap(context);

    if (!mProgram.rayTracing().isEmpty())
        return createRayTracingExplicit(context);

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

bool D3DPipeline::createRayTracingExplicit(D3DContext &context)
{
    const auto &config = mProgram.rayTracing();
    const auto fail = [&](const QString &text) {
        mMessages.insert(mItemId, MessageType::ScriptError,
            "DXR records: " + text);
        return false;
    };
    if (config.contains("_invalidType"))
        return fail("rayTracing must be a JSON object");
    for (const auto key : { "hitGroups", "hitRecords", "missRecords" })
        if (config.value(key).typeId() != QMetaType::QVariantList)
            return fail(QStringLiteral("%1 must be an array").arg(key));

    auto shaders = std::map<std::wstring, const D3DShader *>();
    for (const auto &shader : mProgram.shaders()) {
        if (!isRayTracingShaderType(shader.type()))
            return fail("Explicit DXR programs may contain only ray stages and "
                        "includables");
        if (!shaders.emplace(shader.entryPoint().toStdWString(), &shader)
                .second)
            return fail("Duplicate export: " + shader.entryPoint());
    }
    const auto hasStage = [&](const std::wstring &name,
                              Shader::ShaderType type) {
        const auto it = shaders.find(name);
        return it != shaders.end() && it->second->type() == type;
    };
    const auto raygen = config.value("rayGenerationRecord").toMap();
    if (raygen.contains("arguments"))
        return fail(
            "Local arguments are currently supported on hit records only");
    const auto raygenName = raygen.value("export").toString().toStdWString();
    if (!hasStage(raygenName, Shader::ShaderType::RayGeneration))
        return fail("rayGenerationRecord must select a raygeneration export");

    struct Group
    {
        std::wstring name, closestHit, anyHit, intersection;
        bool procedural;
    };
    auto groups = std::vector<Group>();
    auto groupNames = std::set<std::wstring>();
    for (const auto &value : config.value("hitGroups").toList()) {
        const auto item = value.toMap();
        const auto type = item.value("type").toString();
        if (type != "Triangles" && type != "Procedural")
            return fail("Hit-group type must be Triangles or Procedural");
        auto group = Group{
            item.value("name").toString().toStdWString(),
            item.value("closestHit").toString().toStdWString(),
            item.value("anyHit").toString().toStdWString(),
            item.value("intersection").toString().toStdWString(),
            type == "Procedural",
        };
        if (group.name.empty() || shaders.contains(group.name)
            || !groupNames.insert(group.name).second)
            return fail("Empty, duplicate or conflicting hit-group name");
        if ((!group.closestHit.empty()
                && !hasStage(group.closestHit,
                    Shader::ShaderType::RayClosestHit))
            || (!group.anyHit.empty()
                && !hasStage(group.anyHit, Shader::ShaderType::RayAnyHit))
            || (group.procedural
                && !hasStage(group.intersection,
                    Shader::ShaderType::RayIntersection))
            || (!group.procedural && !group.intersection.empty()))
            return fail(
                "Hit-group imports do not match their required shader stages");
        groups.push_back(std::move(group));
    }

    struct Record
    {
        std::wstring name;
        std::vector<UINT> arguments;
    };
    const auto readRecords = [&](const char *key, bool hitTable,
                                 std::vector<Record> &records) {
        for (const auto &value : config.value(key).toList()) {
            const auto record = value.toMap();
            const auto name = record.value("export").toString().toStdWString();
            if (hitTable ? !groupNames.contains(name)
                         : !hasStage(name, Shader::ShaderType::RayMiss))
                return fail(
                    QStringLiteral("Unknown or wrong-stage export in %1")
                        .arg(key));
            auto result = Record{ name, { } };
            const auto rootIt = mGroupLocalRoots.find(name);
            const auto local = hitTable && rootIt != mGroupLocalRoots.end()
                ? rootIt->second
                : nullptr;
            if (record.contains("arguments")
                && record.value("arguments").typeId() != QMetaType::QVariantMap)
                return fail(
                    "Record arguments must be an object of DWORD arrays");
            const auto arguments = record.value("arguments").toMap();
            if (!local && !arguments.isEmpty())
                return fail(
                    "Arguments require a hit group with a local-root schema");
            if (local) {
                if (arguments.size()
                    != static_cast<qsizetype>(local->constants.size()))
                    return fail("Record arguments must match the local "
                                "parameter names");
                // schema order determines root-parameter order
                for (const auto &parameter : local->constants) {
                    const auto values = arguments.value(parameter.name);
                    if (values.typeId() != QMetaType::QVariantList
                        || values.toList().size() != parameter.count)
                        return fail("Local constant argument count mismatch: "
                            + parameter.name);
                    for (const auto &value : values.toList()) {
                        auto word = UINT{ };
                        if (!readDword(value, word))
                            return fail("Local constants require "
                                        "unsigned32-bit integer values");
                        result.arguments.push_back(word);
                    }
                }
            }
            records.push_back(std::move(result));
        }
        return true;
    };
    auto missRecords = std::vector<Record>();
    auto hitRecords = std::vector<Record>();
    if (!readRecords("missRecords", false, missRecords)
        || !readRecords("hitRecords", true, hitRecords))
        return false;
    if (missRecords.empty())
        return fail(
            "At least one Miss record is required by this host interface");

    auto device = ComPtr<ID3D12Device5>();
    if (FAILED(context.device.QueryInterface(IID_PPV_ARGS(&device))))
        return fail("ID3D12Device5 is unavailable");
    auto stateObject =
        CD3DX12_STATE_OBJECT_DESC(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);
    for (const auto &[name, shader] : shaders) {
        const auto bytecode = D3D12_SHADER_BYTECODE{
            shader->binary()->GetBufferPointer(),
            shader->binary()->GetBufferSize(),
        };
        auto library =
            stateObject.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        library->SetDXILLibrary(&bytecode);
        library->DefineExport(name.c_str());
    }
    for (const auto &group : groups) {
        auto desc = stateObject.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        desc->SetHitGroupExport(group.name.c_str());
        desc->SetHitGroupType(group.procedural
                ? D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE
                : D3D12_HIT_GROUP_TYPE_TRIANGLES);
        if (!group.closestHit.empty())
            desc->SetClosestHitShaderImport(group.closestHit.c_str());
        if (!group.anyHit.empty())
            desc->SetAnyHitShaderImport(group.anyHit.c_str());
        if (!group.intersection.empty())
            desc->SetIntersectionShaderImport(group.intersection.c_str());
    }
    auto shaderConfig =
        stateObject
            .CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    shaderConfig->Config(128, D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES);
    auto root =
        stateObject.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    root->SetRootSignature(mRootSignature.Get());
    for (auto &[name, local] : mLocalRoots) {
        if (local.exports.empty())
            continue;
        auto parameters = std::vector<CD3DX12_ROOT_PARAMETER>();
        for (const auto &constant : local.constants)
            parameters.emplace_back().InitAsConstants(constant.count,
                constant.shaderRegister, constant.registerSpace);
        const auto desc = CD3DX12_ROOT_SIGNATURE_DESC(
            static_cast<UINT>(parameters.size()), parameters.data(), 0, nullptr,
            D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
        auto blob = ComPtr<ID3DBlob>();
        if (FAILED(D3D12SerializeRootSignature(&desc,
                D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr))
            || FAILED(context.device.CreateRootSignature(0,
                blob->GetBufferPointer(), blob->GetBufferSize(),
                IID_PPV_ARGS(&local.signature))))
            return fail("Cannot create local root: " + name);
        auto signature =
            stateObject
                .CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
        signature->SetRootSignature(local.signature.Get());
        auto association = stateObject.CreateSubobject<
            CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
        association->SetSubobjectToAssociate(*signature);
        for (const auto &exportName : local.exports)
            association->AddExport(exportName.c_str());
    }
    auto pipeline =
        stateObject
            .CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pipeline->Config(1);
    if (FAILED(device->CreateStateObject(stateObject,
            IID_PPV_ARGS(&mRayTracingStateObject))))
        return fail("CreateStateObject failed for configured exports/groups");
    auto properties = ComPtr<ID3D12StateObjectProperties>();
    if (FAILED(mRayTracingStateObject.As(&properties)))
        return fail("State-object properties unavailable");

    constexpr auto identifierSize =
        UINT64{ D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES };
    const auto recordStride = [&](const std::vector<Record> &records) {
        auto size = identifierSize;
        for (const auto &record : records)
            size = std::max(size,
                identifierSize + record.arguments.size() * sizeof(UINT));
        constexpr auto alignment =
            UINT64{ D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT };
        return (size + alignment - 1) & ~(alignment - 1);
    };
    const auto missStride = recordStride(missRecords);
    const auto hitStride = recordStride(hitRecords);
    const auto alignTable = [](UINT64 size) {
        constexpr auto alignment =
            UINT64{ D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT };
        return (size + alignment - 1) & ~(alignment - 1);
    };
    const auto missOffset = alignTable(identifierSize);
    const auto hitOffset =
        alignTable(missOffset + missRecords.size() * missStride);
    const auto bufferSize =
        alignTable(hitOffset + hitRecords.size() * hitStride);
    const auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto buffer = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    if (FAILED(context.device.CreateCommittedResource(&heap,
            D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&mShaderBindingTable))))
        return fail("Shader-table allocation failed");
    auto data = std::add_pointer_t<void>{ };
    if (FAILED(mShaderBindingTable->Map(0, nullptr, &data)))
        return fail("Shader-table mapping failed");
    std::memset(data, 0, static_cast<size_t>(bufferSize));
    const auto copy = [&](UINT64 offset, const Record &record) {
        const auto identifier =
            properties->GetShaderIdentifier(record.name.c_str());
        if (!identifier)
            return false;
        auto destination = static_cast<std::byte *>(data) + offset;
        std::memcpy(destination, identifier, identifierSize);
        if (!record.arguments.empty())
            std::memcpy(destination + identifierSize, record.arguments.data(),
                record.arguments.size() * sizeof(UINT));
        return true;
    };
    auto valid = copy(0, Record{ raygenName, { } });
    for (auto i = size_t{ }; i < missRecords.size(); ++i)
        valid &= copy(missOffset + i * missStride, missRecords[i]);
    for (auto i = size_t{ }; i < hitRecords.size(); ++i)
        valid &= copy(hitOffset + i * hitStride, hitRecords[i]);
    mShaderBindingTable->Unmap(0, nullptr);
    if (!valid)
        return fail("Configured export has no shader identifier");

    const auto address = mShaderBindingTable->GetGPUVirtualAddress();
    mRayGenerationShaderRecord = { address, identifierSize };
    mMissShaderTable = { address + missOffset, missRecords.size() * missStride,
        missStride };
    mHitGroupTable = hitRecords.empty()
        ? D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{ }
        : D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{ address + hitOffset,
              hitRecords.size() * hitStride, hitStride };
    return true;
}

bool D3DPipeline::prepareLocalRoots()
{
    const auto &config = mProgram.rayTracing();
    const auto fail = [&](const QString &text) {
        mMessages.insert(mItemId, MessageType::ScriptError,
            "DXR local roots: " + text);
        return false;
    };
    if (config.contains("localRoots")
        && config.value("localRoots").typeId() != QMetaType::QVariantList)
        return fail("localRoots must be an array");
    for (const auto &value : config.value("localRoots").toList()) {
        const auto item = value.toMap();
        const auto name = item.value("name").toString();
        if (name.isEmpty() || mLocalRoots.contains(name))
            return fail("Empty or duplicate schema name");
        if (item.value("parameters").typeId() != QMetaType::QVariantList)
            return fail("Schema parameters must be an array");
        auto root = LocalRoot{ };
        auto names = std::set<QString>();
        auto registers = std::set<std::pair<UINT, UINT>>();
        auto words = UINT{ };
        for (const auto &entry : item.value("parameters").toList()) {
            const auto parameter = entry.toMap();
            auto constant =
                LocalConstant{ parameter.value("name").toString(), 0, 0, 0 };
            if (parameter.value("type").toString() != "Constants")
                return fail("Only Constants parameters are supported");
            if (constant.name.isEmpty() || !names.insert(constant.name).second
                || !readDword(parameter.value("shaderRegister"),
                    constant.shaderRegister)
                || !readDword(parameter.value("registerSpace"),
                    constant.registerSpace)
                || !readDword(parameter.value("count"), constant.count)
                || !constant.count || constant.count > 1016
                || !registers
                    .emplace(constant.registerSpace, constant.shaderRegister)
                    .second)
                return fail("Invalid or duplicate local constant parameter");
            words += constant.count;
            if (words > 1016)
                return fail("Local arguments exceed the4064-byte DXR limit");
            root.constants.push_back(std::move(constant));
        }
        if (root.constants.empty())
            return fail(
                "Local schemas must contain at least one constant parameter");
        mLocalRoots.emplace(name, std::move(root));
    }
    for (const auto &value : config.value("hitGroups").toList()) {
        const auto group = value.toMap();
        const auto name = group.value("name").toString().toStdWString();
        const auto localName = group.value("localRoot").toString();
        auto local = std::add_pointer_t<LocalRoot>{ };
        if (!localName.isEmpty()) {
            const auto it = mLocalRoots.find(localName);
            if (it == mLocalRoots.end())
                return fail("Unknown local schema: " + localName);
            local = &it->second;
            local->exports.push_back(name);
            mGroupLocalRoots.emplace(name, local);
        }
        // shared components require matching local schemas across groups
        for (const auto key : { "closestHit", "anyHit", "intersection" }) {
            const auto shader = group.value(key).toString().toStdWString();
            if (shader.empty())
                continue;
            const auto [it, inserted] =
                mShaderLocalRoots.emplace(shader, local);
            if (!inserted && it->second != local)
                return fail(
                    "Shared shader has conflicting local-root associations");
        }
    }
    return true;
}

const D3DPipeline::LocalConstant *D3DPipeline::localConstant(
    const D3DShader &shader, const D3D12_SHADER_INPUT_BIND_DESC &binding) const
{
    if (binding.Type != D3D_SIT_CBUFFER)
        return nullptr;
    const auto it = mShaderLocalRoots.find(shader.entryPoint().toStdWString());
    if (it == mShaderLocalRoots.end() || !it->second)
        return nullptr;
    for (const auto &constant : it->second->constants)
        if (constant.shaderRegister == binding.BindPoint
            && constant.registerSpace == binding.Space)
            return &constant;
    return nullptr;
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
            if (const auto constant = localConstant(shader, bindDesc)) {
                // constants use member bytes without tail padding
                auto usedBytes = uint32_t{ };
                for (auto i = 0u; i < binding.block.member_count; ++i) {
                    const auto &member = binding.block.members[i];
                    usedBytes =
                        std::max(usedBytes, member.offset + member.size);
                }
                if (bindDesc.BindCount != 1
                    || usedBytes > constant->count * 4u) {
                    mMessages.insert(mItemId, MessageType::ScriptError,
                        "DXR local constant schema does not cover the "
                        "reflected cbuffer");
                    return false;
                }
                continue;
            }
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

    // exclude local registers from the global root
    for (const auto &[name, local] : mLocalRoots)
        if (!local.exports.empty())
            for (const auto &constant : local.constants)
                if (resources.contains({ D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                        constant.registerSpace, constant.shaderRegister })) {
                    mMessages.insert(mItemId, MessageType::ScriptError,
                        "DXR global/local constant registers overlap");
                    return false;
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

    if (!setDescriptors(context, scriptEngine))
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
