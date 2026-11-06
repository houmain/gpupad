
#include "VKPipeline.h"
#include "VKBuffer.h"
#include "VKProgram.h"
#include "VKStream.h"
#include "VKTarget.h"
#include "VKTexture.h"
#include "VKAccelerationStructure.h"
#include <QScopeGuard>

namespace {
    const auto maxVariableBindGroupEntries = 128;

    bool hasRecordDataBlockName(const SpvReflectBlockVariable &block,
        const QString &name)
    {
        return name == QString::fromUtf8(block.name ? block.name : "")
            || name == getBufferBlockTypeName(block);
    }

    KDGpu::ResourceBindingType getResourceType(SpvReflectDescriptorType type)
    {
        return static_cast<KDGpu::ResourceBindingType>(type);
    }

    bool isRayTracingStage(KDGpu::ShaderStageFlagBits stage)
    {
        using Stage = KDGpu::ShaderStageFlagBits;
        switch (stage) {
        case Stage::RaygenBit:
        case Stage::AnyHitBit:
        case Stage::ClosestHitBit:
        case Stage::MissBit:
        case Stage::IntersectionBit:
        case Stage::CallableBit:     return true;
        default:                     return false;
        }
    }

    KDGpu::SamplerOptions getSamplerOptions(const SamplerBinding &binding,
        float maxAnisotropy)
    {
        const auto getFilter = [](Binding::Filter filter) {
            switch (filter) {
            case Binding::Filter::Linear:
            case Binding::Filter::LinearMipMapNearest:
            case Binding::Filter::LinearMipMapLinear:
                return KDGpu::FilterMode::Linear;
            default: return KDGpu::FilterMode::Nearest;
            }
        };

        const auto getMipmapFilter = [](Binding::Filter filter) {
            switch (filter) {
            case Binding::Filter::NearestMipMapLinear:
            case Binding::Filter::LinearMipMapLinear:
                return KDGpu::MipmapFilterMode::Linear;
            default: return KDGpu::MipmapFilterMode::Nearest;
            }
        };

        const auto getAddressMode = [](Binding::WrapMode mode) {
            switch (mode) {
            case Binding::WrapMode::Repeat: return KDGpu::AddressMode::Repeat;
            case Binding::WrapMode::MirroredRepeat:
                return KDGpu::AddressMode::MirroredRepeat;
            case Binding::WrapMode::ClampToEdge:
                return KDGpu::AddressMode::ClampToEdge;
            case Binding::WrapMode::ClampToBorder:
                return KDGpu::AddressMode::ClampToBorder;
                //case Binding::WrapMode::MirrorClampToEdge: return KDGpu::AddressMode::MirrorClampToEdge;
            }
            Q_UNREACHABLE();
            return KDGpu::AddressMode{ };
        };

        return KDGpu::SamplerOptions{
            .magFilter = getFilter(binding.magFilter),
            .minFilter = getFilter(binding.minFilter),
            .mipmapFilter = getMipmapFilter(binding.minFilter),
            .u = getAddressMode(binding.wrapModeX),
            .v = getAddressMode(binding.wrapModeY),
            .w = getAddressMode(binding.wrapModeZ),
            // .lodMinClamp = 0.0f,
            // .lodMaxClamp = MipmapLodClamping::NoClamping,
            .anisotropyEnabled = binding.anisotropic,
            .maxAnisotropy = maxAnisotropy,
            .compareEnabled = (binding.comparisonFunc
                != Binding::ComparisonFunc::NoComparisonFunc),
            .compare = toKDGpu(binding.comparisonFunc),
            //.normalizedCoordinates = true
        };
    }

    uint32_t getMaxBindingNumberInSet(const Reflection &reflection,
        uint32_t set)
    {
        auto maxBindingNumber = 0u;
        for (const auto &desc : reflection.descriptorBindings())
            if (desc.set == set)
                maxBindingNumber = std::max(maxBindingNumber, desc.binding);

        return maxBindingNumber;
    }
} // namespace

VKPipeline::VKPipeline(ItemId itemId, VKProgram *program,
    const ShaderTable *shaderTable)
    : PipelineBase(itemId)
    , mProgram(*program)
    , mShaderTable(shaderTable)
{
}

VKPipeline::~VKPipeline() = default;

std::optional<KDGpu::RenderPassCommandRecorder> VKPipeline::beginRenderPass(
    VKContext &context, bool flipViewport)
{
    if (mVertexStream)
        for (auto &buffer : mVertexStream->getBuffers())
            buffer->prepareVertexBuffer(context);

    auto passOptions = KDGpu::RenderPassCommandRecorderOptions{ };
    if (!mTarget->prepare(context, &passOptions))
        return std::nullopt;
    auto renderPass = context.commandRecorder->beginRenderPass(passOptions);
    renderPass.setPipeline(mGraphicsPipeline);

    if (flipViewport) {
        // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
        renderPass.setViewport({
            .x = 0,
            .y = static_cast<float>(passOptions.framebufferHeight),
            .width = static_cast<float>(passOptions.framebufferWidth),
            .height = -static_cast<float>(passOptions.framebufferHeight),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        });
    }

    for (auto i = 0u; i < mBindGroups.size(); ++i)
        if (mBindGroups[i].bindGroup.isValid())
            renderPass.setBindGroup(i, mBindGroups[i].bindGroup);

    if (mVertexStream) {
        const auto &buffers = mVertexStream->getBuffers();
        const auto &offsets = mVertexStream->getBufferOffsets();
        for (auto i = 0u; i < buffers.size(); ++i)
            renderPass.setVertexBuffer(i, buffers[i]->buffer(),
                static_cast<KDGpu::DeviceSize>(offsets[i]));
    }
    return renderPass;
}

KDGpu::ComputePassCommandRecorder VKPipeline::beginComputePass(
    VKContext &context)
{
    auto computePass = context.commandRecorder->beginComputePass();
    computePass.setPipeline(mComputePipeline);

    for (auto i = 0u; i < mBindGroups.size(); ++i)
        if (mBindGroups[i].bindGroup.isValid())
            computePass.setBindGroup(i, mBindGroups[i].bindGroup);

    return computePass;
}

KDGpu::RayTracingPassCommandRecorder VKPipeline::beginRayTracingPass(
    VKContext &context)
{
    auto rayTracingPass = context.commandRecorder->beginRayTracingPass();
    rayTracingPass.setPipeline(mRayTracingPipeline);

    for (auto i = 0u; i < mBindGroups.size(); ++i)
        if (mBindGroups[i].bindGroup.isValid())
            rayTracingPass.setBindGroup(i, mBindGroups[i].bindGroup);

    return rayTracingPass;
}

namespace KDGpu {
    bool operator<(const SamplerOptions &a, const SamplerOptions &b)
    {
        const auto tie = [](const SamplerOptions &a) {
            return std::tie(a.magFilter, a.minFilter, a.mipmapFilter, a.u, a.v,
                a.w, a.lodMinClamp, a.lodMaxClamp, a.anisotropyEnabled,
                a.maxAnisotropy, a.compareEnabled, a.compare,
                a.normalizedCoordinates);
        };
        return tie(a) < tie(b);
    }
} // namespace KDGpu

const KDGpu::Sampler &VKPipeline::getSampler(VKContext &context,
    const SamplerBinding &samplerBinding)
{
    const auto maxSamplerAnisotropy =
        context.device.adapter()->properties().limits.maxSamplerAnisotropy;
    const auto samplerOptions =
        getSamplerOptions(samplerBinding, maxSamplerAnisotropy);

    auto &sampler = mSamplers[samplerOptions];
    if (!sampler.isValid())
        sampler = context.device.createSampler(samplerOptions);
    return sampler;
}

auto VKPipeline::getBindGroup(uint32_t set) -> BindGroup &
{
    mBindGroups.resize(
        std::max(set + 1, static_cast<uint32_t>(mBindGroups.size())));
    return mBindGroups[set];
}

bool VKPipeline::createOrUpdateBindGroup(uint32_t set, uint32_t binding,
    const KDGpu::ResourceBindingLayout &layout)
{
    auto &bindGroup = getBindGroup(set);
    auto &bindings = bindGroup.bindings;

    const auto it = std::find_if(begin(bindings), end(bindings),
        [&](const KDGpu::ResourceBindingLayout &layout) {
            return layout.binding == binding;
        });
    if (it != end(bindings)) {
        // TODO: improve check
        if (it->resourceType != layout.resourceType) {
            mMessages.insert(mProgram.itemId(),
                MessageType::IncompatibleBindings,
                QStringLiteral("(%1/%2)").arg(set).arg(binding));
            return false;
        }
        it->shaderStages |= layout.shaderStages;
    } else {
        bindings.push_back(layout);
    }
    return true;
}

void VKPipeline::setBindGroupResource(uint32_t set, bool isVariableLengthArray,
    KDGpu::BindGroupEntry &&resource)
{
    auto &bindGroup = getBindGroup(set);

    if (isVariableLengthArray)
        bindGroup.maxVariableArrayLength = std::max(
            bindGroup.maxVariableArrayLength, resource.arrayElement + 1);

    for (const auto &res : bindGroup.resources)
        if (res.binding == resource.binding) {
            // TODO: improve check
            if (res.resource.type() != resource.resource.type()) {
                mMessages.insert(mProgram.itemId(),
                    MessageType::IncompatibleBindings,
                    QStringLiteral("(set %1)").arg(set));
                return;
            }
            if (resource.arrayElement == res.arrayElement)
                return;
        }
    bindGroup.resources.emplace_back(std::move(resource));
}

auto VKPipeline::getDynamicUniformBuffer(uint32_t set, uint32_t binding,
    uint32_t arrayElement) -> DynamicUniformBuffer &
{
    auto it = std::find_if(mDynamicUniformBuffers.begin(),
        mDynamicUniformBuffers.end(), [&](const auto &block) {
            return (block->set == set && block->binding == binding
                && block->arrayElement == arrayElement);
        });
    if (it == mDynamicUniformBuffers.end()) {
        it = mDynamicUniformBuffers.emplace(mDynamicUniformBuffers.end(),
            new DynamicUniformBuffer{
                .set = set,
                .binding = binding,
                .arrayElement = arrayElement,
            });
    }
    return **it;
}

bool VKPipeline::hasPushConstants() const
{
    return (mPushConstantRange.size > 0);
}

bool VKPipeline::updatePushConstants(ScriptEngine &scriptEngine)
{
    for (const auto &[stage, reflection] : mProgram.reflection())
        for (const auto &block : reflection.pushConstantBlocks()) {
            if (const auto bufferBinding = find(mBindings.buffers,
                    block.type_description->type_name)) {
                if (!bufferBinding->buffer) {
                    mMessages.insert(mItemId, MessageType::BufferNotSet,
                        block.type_description->type_name);
                    return false;
                }

                auto &buffer = static_cast<VKBuffer &>(*bufferBinding->buffer);
                mUsedItems += bufferBinding->bindingItemId;
                mUsedItems += bufferBinding->blockItemId;
                mUsedItems += buffer.usedItems();

                buffer.reload();
                const auto &bufferData = buffer.data();
                auto &constantData = mPushConstantData;
                const auto size = std::min(constantData.size(),
                    static_cast<size_t>(bufferData.size()));
                std::memcpy(constantData.data(), bufferData.constData(), size);
                if (size < constantData.size())
                    std::memset(mPushConstantData.data() + size, 0x00,
                        constantData.size() - size);
            } else {
                if (!applyBufferMemberBindings(mPushConstantData, block, 0,
                        scriptEngine))
                    return false;
            }
        }
    return true;
}

bool VKPipeline::updatePushConstants(
    KDGpu::RenderPassCommandRecorder &renderPass, ScriptEngine &scriptEngine)
{
    if (hasPushConstants()) {
        if (!updatePushConstants(scriptEngine))
            return false;
        renderPass.pushConstant(mPushConstantRange, mPushConstantData.data());
    }
    return true;
}

bool VKPipeline::updatePushConstants(
    KDGpu::ComputePassCommandRecorder &computePass, ScriptEngine &scriptEngine)
{
    if (hasPushConstants()) {
        if (!updatePushConstants(scriptEngine))
            return false;
        computePass.pushConstant(mPushConstantRange, mPushConstantData.data());
    }
    return true;
}

bool VKPipeline::updatePushConstants(
    KDGpu::RayTracingPassCommandRecorder &rayTracingPass,
    ScriptEngine &scriptEngine)
{
    if (hasPushConstants()) {
        if (!updatePushConstants(scriptEngine))
            return false;
        rayTracingPass.pushConstant(mPushConstantRange,
            mPushConstantData.data());
    }
    return true;
}

bool VKPipeline::createGraphics(VKContext &context,
    KDGpu::PrimitiveOptions &primitiveOptions, VKTarget *target,
    VKStream *vertexStream)
{
    if (std::exchange(mCreated, true))
        return mGraphicsPipeline.isValid();

    mTarget = target;
    mVertexStream = vertexStream;

    if (!mTarget->validateAttachments(context))
        return false;

    if (!createLayout(context))
        return false;

    auto vertexOptions = KDGpu::VertexOptions{ };
    if (mVertexStream) {
        vertexOptions = mVertexStream->getVertexOptions();
        if (vertexOptions.attributes.empty())
            return false;
    }

    mGraphicsPipeline = context.device.createGraphicsPipeline({
        .shaderStages = mProgram.getShaderStages(),
        .layout = mPipelineLayout,
        .vertex = vertexOptions,
        .renderTargets = mTarget->getRenderTargetOptions(),
        .depthStencil = mTarget->getDepthStencilOptions(),
        .primitive = primitiveOptions,
        .multisample = mTarget->getMultisampleOptions(),
    });

    if (!mGraphicsPipeline.isValid())
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);

    return mGraphicsPipeline.isValid();
}

bool VKPipeline::createCompute(VKContext &context)
{
    if (std::exchange(mCreated, true))
        return mComputePipeline.isValid();

    if (!createLayout(context))
        return false;

    const auto stages = mProgram.getShaderStages();
    if (stages.size() != 1
        || stages[0].stage != KDGpu::ShaderStageFlagBits::ComputeBit)
        return false;

    mComputePipeline = context.device.createComputePipeline({
        .layout = mPipelineLayout,
        .shaderStage = { 
            .shaderModule = stages[0].shaderModule,
            .entryPoint = stages[0].entryPoint, 
        },
    });
    return mComputePipeline.isValid();
}

bool VKPipeline::createRayTracing(VKContext &context,
    ScriptEngine &scriptEngine)
{
    if (std::exchange(mCreated, true))
        return mRayTracingPipeline.isValid();

    if (!createLayout(context))
        return false;

    // https://www.willusher.io/graphics/2019/11/20/the-sbt-three-ways/
    auto options = KDGpu::RayTracingPipelineOptions{ };
    options.shaderStages = mProgram.getShaderStages();
    options.layout = mPipelineLayout;

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
                               const QString &role) -> const VKShader * {
        const auto shader = mProgram.getShader(shaderId);
        if (!shader || shader->type() != type) {
            fail(shaderId ? shaderId : mShaderTable->id,
                QStringLiteral("%1 has an invalid shader reference")
                    .arg(role));
            return nullptr;
        }
        return shader;
    };

    auto rayGenerationGroup = std::optional<size_t>();
    auto hitGroups = std::vector<size_t>();
    auto missGroups = std::vector<size_t>();
    for (const auto item : mShaderTable->items) {
        const auto record = castItem<Record>(item);
        if (!record)
            continue;
        mUsedItems += record->id;
        switch (record->recordType) {
        case Record::RecordType::RayGeneration: {
            if (rayGenerationGroup)
                return fail(record->id,
                    "A shader table must contain exactly one ray-generation record");
            const auto shader = getShader(record->shaderId,
                Shader::ShaderType::RayGeneration,
                "Ray-generation record");
            if (!shader)
                return false;
            rayGenerationGroup = options.shaderGroups.size();
            mRayGenerationRecord = record;
            options.shaderGroups.push_back({
                .type = KDGpu::RayTracingShaderGroupType::General,
                .generalShaderIndex = shader->shaderIndex(),
            });
            break;
        }

        case Record::RecordType::Miss: {
            const auto shader = getShader(record->shaderId,
                Shader::ShaderType::RayMiss, "Miss record");
            if (!shader)
                return false;
            missGroups.push_back(options.shaderGroups.size());
            mMissRecords.push_back(record);
            options.shaderGroups.push_back({
                .type = KDGpu::RayTracingShaderGroupType::General,
                .generalShaderIndex = shader->shaderIndex(),
            });
            break;
        }

        case Record::RecordType::Hit: {
            const auto intersection = record->intersectionShaderId
                ? getShader(record->intersectionShaderId,
                      Shader::ShaderType::RayIntersection,
                      "Intersection shader")
                : nullptr;
            const auto anyHit = record->anyHitShaderId
                ? getShader(record->anyHitShaderId,
                      Shader::ShaderType::RayAnyHit, "Any-hit shader")
                : nullptr;
            const auto closestHit = record->closestHitShaderId
                ? getShader(record->closestHitShaderId,
                      Shader::ShaderType::RayClosestHit,
                      "Closest-hit shader")
                : nullptr;
            if ((record->intersectionShaderId && !intersection)
                || (record->anyHitShaderId && !anyHit)
                || (record->closestHitShaderId && !closestHit))
                return false;
            hitGroups.push_back(options.shaderGroups.size());
            mHitRecords.push_back(record);
            options.shaderGroups.push_back({
                .type = intersection
                    ? KDGpu::RayTracingShaderGroupType::ProceduralHit
                    : KDGpu::RayTracingShaderGroupType::TrianglesHit,
            });
            auto &shaderGroup = options.shaderGroups.back();
            if (intersection)
                shaderGroup.intersectionShaderIndex = intersection->shaderIndex();
            if (anyHit)
                shaderGroup.anyHitShaderIndex = anyHit->shaderIndex();
            if (closestHit)
                shaderGroup.closestHitShaderIndex = closestHit->shaderIndex();
            break;
        }
        }
    }
    if (!rayGenerationGroup)
        return fail(mShaderTable->id,
            "A shader table must contain exactly one ray-generation record");

    auto validRecordData = true;
    mRayGenerationRecordDataSize = getRecordData(
        *mRayGenerationRecord, scriptEngine, &validRecordData).size();
    for (const auto record : mMissRecords)
        mMissRecordDataSize = std::max(mMissRecordDataSize,
            getRecordData(*record, scriptEngine, &validRecordData).size());
    for (const auto record : mHitRecords)
        mHitRecordDataSize = std::max(mHitRecordDataSize,
            getRecordData(*record, scriptEngine, &validRecordData).size());
    if (!validRecordData)
        return false;

    mRayTracingPipeline = context.device.createRayTracingPipeline(options);
    if (!mRayTracingPipeline.isValid())
        return false;

    auto &sbt = mRayTracingShaderBindingTable;
    sbt = KDGpu::RayTracingShaderBindingTable(&context.device,
        KDGpu::RayTracingShaderBindingTableOptions{
            .nbrMissShaders =
                static_cast<uint32_t>(missGroups.size()),
            .nbrHitShaders =
                static_cast<uint32_t>(hitGroups.size()),
            .rayGenRecordDataSize = mRayGenerationRecordDataSize,
            .missRecordDataSize = mMissRecordDataSize,
            .hitRecordDataSize = mHitRecordDataSize,
        });
    sbt.addRayGenShaderGroup(mRayTracingPipeline,
        static_cast<uint32_t>(*rayGenerationGroup));
    for (auto entry = size_t{}; entry < hitGroups.size(); ++entry)
        sbt.addHitShaderGroup(mRayTracingPipeline,
            static_cast<uint32_t>(hitGroups[entry]),
            static_cast<uint32_t>(entry));
    for (auto entry = size_t{}; entry < missGroups.size(); ++entry)
        sbt.addMissShaderGroup(mRayTracingPipeline,
            static_cast<uint32_t>(missGroups[entry]),
            static_cast<uint32_t>(entry));
    return true;
}

std::vector<std::byte> VKPipeline::getRecordData(const Record &record,
    ScriptEngine &scriptEngine, bool *valid)
{
    const auto getShaders = [&]() {
        auto result = std::vector<const VKShader *>();
        const auto add = [&](ItemId shaderId) {
            if (const auto shader = mProgram.getShader(shaderId))
                result.push_back(shader);
        };
        if (record.recordType == Record::RecordType::Hit) {
            add(record.intersectionShaderId);
            add(record.anyHitShaderId);
            add(record.closestHitShaderId);
        } else {
            add(record.shaderId);
        }
        return result;
    };
    const auto shaders = getShaders();
    auto result = std::vector<std::byte>();
    for (const auto item : record.items) {
        const auto binding = castItem<Binding>(item);
        if (!binding)
            continue;
        mUsedItems += binding->id;
        if (binding->bindingType != Binding::BindingType::Uniform) {
            mMessages.insert(binding->id, MessageType::ScriptError,
                "Shader-table records currently support only Uniform bindings");
            *valid = false;
            continue;
        }

        auto incompatibleLayout = false;
        const auto block = [&]() -> const SpvReflectBlockVariable * {
            auto found = static_cast<const SpvReflectBlockVariable *>(nullptr);
            auto foundLayout = ShaderRecordLayout{};
            for (const auto shader : shaders)
                for (const auto candidate :
                    shader->reflection().shaderRecordBlocks())
                    if (hasRecordDataBlockName(*candidate, binding->name)) {
                        const auto candidateLayout =
                            getShaderRecordLayout(*candidate);
                        if (found && foundLayout != candidateLayout) {
                            mMessages.insert(binding->id,
                                MessageType::ScriptError,
                                "Record-data block has incompatible layouts");
                            *valid = false;
                            incompatibleLayout = true;
                            return nullptr;
                        }
                        found = candidate;
                        foundLayout = candidateLayout;
                    }
            return found;
        }();
        if (!block) {
            if (!incompatibleLayout)
                mMessages.insert(binding->id, MessageType::ScriptError,
                    QStringLiteral("No shader record block named '%1'")
                        .arg(binding->name));
            *valid = false;
            continue;
        }

        const auto layout = getShaderRecordLayout(*block);
        const auto values =
            mBindings.evaluatedUniformValues.value(binding->id);
        checkValueCount(values.size(), -1,
            static_cast<int>(layout.elements.size()), binding->id,
            scriptEngine.messages());
        const auto data = packShaderRecordData(layout, values);
        result.insert(result.end(), data.begin(), data.end());
    }
    return result;
}

bool VKPipeline::updateRayTracingShaderBindingTable(
    ScriptEngine &scriptEngine)
{
    auto valid = true;
    const auto update = [&](const Record &record, size_t capacity,
                            const auto &write) {
        const auto data = getRecordData(record, scriptEngine, &valid);
        if (data.size() > capacity) {
            mMessages.insert(record.id, MessageType::ScriptError,
                "Shader-record data grew beyond its allocated size");
            valid = false;
            return;
        }
        write(std::span<const std::byte>(data));
    };
    if (mRayGenerationRecord)
        update(*mRayGenerationRecord, mRayGenerationRecordDataSize,
            [&](auto data) {
                mRayTracingShaderBindingTable.setRayGenRecordData(data);
            });
    for (auto i = size_t{}; i < mMissRecords.size(); ++i)
        update(*mMissRecords[i], mMissRecordDataSize, [&](auto data) {
            mRayTracingShaderBindingTable.setMissRecordData(data,
                static_cast<uint32_t>(i));
        });
    for (auto i = size_t{}; i < mHitRecords.size(); ++i)
        update(*mHitRecords[i], mHitRecordDataSize, [&](auto data) {
            mRayTracingShaderBindingTable.setHitRecordData(data,
                static_cast<uint32_t>(i));
        });
    return valid;
}

bool VKPipeline::createLayout(VKContext &context)
{
    mPushConstantRange = KDGpu::PushConstantRange{ };
    for (const auto &[stage, reflection] : mProgram.reflection()) {
        for (const auto &desc : reflection.descriptorBindings()) {
            if (!desc.accessed)
                continue;

            if (desc.descriptor_type
                    == SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
                && !isRayTracingStage(stage) && !context.features().rayQuery) {
                mMessages.insert(mItemId, MessageType::FeatureNotSupported,
                    "Ray query");
                return false;
            }

            auto count = getBindingArraySize(desc.array);
            auto flags = KDGpu::ResourceBindingFlagBits::None;
            if (count == 0) {
                const auto maxBinding =
                    getMaxBindingNumberInSet(reflection, desc.set);
                if (desc.binding != maxBinding) {
                    mMessages.insert(mItemId,
                        MessageType::OnlyLastBindingMayBeUnsizedArray,
                        QStringLiteral("%1 < %2 in set %3")
                            .arg(desc.binding)
                            .arg(maxBinding)
                            .arg(desc.set));
                    return false;
                }
                count = maxVariableBindGroupEntries;
                flags = KDGpu::ResourceBindingFlagBits::
                    VariableBindGroupEntriesCountBit;
            }

            if (!createOrUpdateBindGroup(desc.set, desc.binding,
                    KDGpu::ResourceBindingLayout{
                        .binding = desc.binding,
                        .count = count,
                        .resourceType = getResourceType(desc.descriptor_type),
                        .shaderStages = stage,
                        .flags = flags,
                    }))
                return false;
        }

        for (const auto &block : reflection.pushConstantBlocks()) {
            mPushConstantRange.shaderStages |= stage;
            mPushConstantRange.size =
                std::max(mPushConstantRange.size, block.size);
        }
    }

    for (const auto &bindGroup : mBindGroups)
        mBindGroupLayouts.emplace_back(context.device.createBindGroupLayout(
            { .bindings = bindGroup.bindings }));

    const auto maxPushConstantsSize =
        context.adapterLimits().maxPushConstantsSize;
    if (mPushConstantRange.size > maxPushConstantsSize) {
        mMessages.insert(mItemId, MessageType::MaxPushConstantSizeExceeded,
            QString::number(maxPushConstantsSize));
        mPushConstantRange.size = maxPushConstantsSize;
    }

    auto options = KDGpu::PipelineLayoutOptions{
        .bindGroupLayouts = { mBindGroupLayouts.begin(),
            mBindGroupLayouts.end() }
    };
    if (mPushConstantRange.size > 0)
        options.pushConstantRanges = { mPushConstantRange };
    mPipelineLayout = context.device.createPipelineLayout(options);
    mPushConstantData.resize(mPushConstantRange.size);
    return true;
}

bool VKPipeline::updateBindings(VKContext &context, ScriptEngine &scriptEngine)
{
    for (auto &bindGroup : mBindGroups) {
        bindGroup.resources = { };
        bindGroup.bindGroup = { };
    }

    auto canRender = true;
    for (const auto &[stage, reflection] : mProgram.reflection())
        for (const auto &desc : reflection.descriptorBindings()) {
            if (!desc.accessed)
                continue;

            auto arrayElement = uint32_t{ };
            forEachArrayElementRec(desc, 0, arrayElement,
                [&](const SpvReflectDescriptorBinding &desc,
                    uint32_t arrayElement, bool *variableLengthArrayDone) {
                    const auto message = updateBindings(context, desc,
                        arrayElement, (variableLengthArrayDone ? true : false),
                        scriptEngine);

                    const auto failed = (message != MessageType::None);
                    if (variableLengthArrayDone && failed) {
                        *variableLengthArrayDone = true;
                        if (message == MessageType::SamplerNotSet
                            || message == MessageType::BufferNotSet)
                            return;
                    }

                    if (failed) {
                        auto name = desc.name;
                        if (isBufferBinding(desc.descriptor_type))
                            name = desc.type_description->type_name;
                        mMessages.insert(mItemId, message, name);
                        canRender = false;
                    }
                });
        }

    if (!canRender)
        return false;

    Q_ASSERT(mBindGroups.size() == mBindGroupLayouts.size());
    for (auto i = 0u; i < mBindGroups.size(); ++i) {
        auto &bindGroup = mBindGroups[i];
        if (bindGroup.resources.empty())
            continue;

        // check that hardcoded limit in layout is not exceeded
        if (bindGroup.maxVariableArrayLength > maxVariableBindGroupEntries) {
            mMessages.insert(mItemId,
                MessageType::MaxVariableBindGroupEntriesExceeded,
                QString::number(maxVariableBindGroupEntries));
            return false;
        }

        bindGroup.bindGroup = context.device.createBindGroup({
            .layout = mBindGroupLayouts[i],
            .resources = bindGroup.resources,
            .maxVariableArrayLength = bindGroup.maxVariableArrayLength,
        });
    }
    return true;
}

MessageType VKPipeline::updateBindings(VKContext &context,
    const SpvReflectDescriptorBinding &desc, uint32_t arrayElement,
    bool isVariableLengthArray, ScriptEngine &scriptEngine)
{
    switch (desc.descriptor_type) {
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        if (const auto bufferBinding =
                find(mBindings.buffers, desc.type_description->type_name)) {
            if (!bufferBinding->buffer)
                return MessageType::BufferNotSet;
            auto &buffer = static_cast<VKBuffer &>(*bufferBinding->buffer);
            mUsedItems += bufferBinding->bindingItemId;
            mUsedItems += bufferBinding->blockItemId;
            mUsedItems += buffer.usedItems();

            buffer.prepareUniformBuffer(context);

            const auto [offset, size] =
                getBufferBindingOffsetSize(*bufferBinding, scriptEngine);
            setBindGroupResource(desc.set, isVariableLengthArray,
                {
                    .binding = desc.binding,
                    .resource =
                        KDGpu::UniformBufferBinding{
                            .buffer = buffer.buffer(),
                            .offset = offset,
                            .size = size,
                        },
                    .arrayElement = arrayElement,
                });
        } else {
            auto &dynamic =
                getDynamicUniformBuffer(desc.set, desc.binding, arrayElement);
            if (!dynamic.buffer.isValid()) {
                dynamic.buffer = context.device.createBuffer({
                    .size = desc.block.size,
                    .usage = KDGpu::BufferUsageFlagBits::UniformBufferBit,
                    .memoryUsage = KDGpu::MemoryUsage::CpuToGpu,
                });
                dynamic.size = desc.block.size;
            }
            Q_ASSERT(dynamic.buffer.isValid());
            Q_ASSERT(desc.block.size == dynamic.size);
            if (!dynamic.buffer.isValid() || desc.block.size != dynamic.size)
                return MessageType::BufferNotSet;

            auto bufferData = std::span<std::byte>(
                static_cast<std::byte *>(dynamic.buffer.map()), dynamic.size);
            const auto guard = qScopeGuard([&] { dynamic.buffer.unmap(); });

            if (!applyBufferMemberBindings(bufferData, desc.block, arrayElement,
                    scriptEngine))
                return MessageType::BufferNotSet;

            setBindGroupResource(desc.set, false,
                {
                    .binding = desc.binding,
                    .resource =
                        KDGpu::UniformBufferBinding{ .buffer = dynamic.buffer },
                    .arrayElement = arrayElement,
                });
        }
        break;

    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
        auto buffer = std::add_pointer_t<VKBuffer>();
        auto offset = uint32_t{ };
        auto size = static_cast<uint32_t>(KDGpu::WholeSize);
        auto bufferBinding =
            find(mBindings.buffers, desc.type_description->type_name);
        if (!bufferBinding)
            bufferBinding = find(mBindings.buffers, desc.name);
        if (desc.type_description->type_name
            == PrintfBase::bufferBindingName()) {
            buffer = &mProgram.printf().getInitializedBuffer(context);

        } else if (bufferBinding) {
            buffer = static_cast<VKBuffer *>(bufferBinding->buffer);
            std::tie(offset, size) =
                getBufferBindingOffsetSize(*bufferBinding, scriptEngine);
            mUsedItems += bufferBinding->bindingItemId;
            mUsedItems += buffer->usedItems();
        }
        if (!buffer)
            return MessageType::BufferNotSet;

        const auto readable =
            !(desc.decoration_flags & SPV_REFLECT_DECORATION_NON_READABLE);
        const auto writeable =
            !(desc.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);
        buffer->prepareShaderStorageBuffer(context, readable, writeable);
        if (writeable)
            addWritableBuffer(buffer);
        setBindGroupResource(desc.set, isVariableLengthArray,
            {
                .binding = desc.binding,
                .resource =
                    KDGpu::StorageBufferBinding{
                        .buffer = buffer->buffer(),
                        .offset = offset,
                        .size = size,
                    },
                .arrayElement = arrayElement,
            });
    } break;

    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
        const auto samplerBinding = find(mBindings.samplers, desc.name);
        if (!samplerBinding)
            return MessageType::SamplerNotSet;

        mUsedItems += samplerBinding->bindingItemId;

        const auto &sampler = getSampler(context, *samplerBinding);

        if (desc.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER) {
            setBindGroupResource(desc.set, isVariableLengthArray,
                {
                    .binding = desc.binding,
                    .resource = KDGpu::SamplerBinding{ .sampler = sampler },
                    .arrayElement = arrayElement,
                });
        } else {
            const auto texture =
                static_cast<VKTexture *>(samplerBinding->texture);
            if (mTarget && mTarget->hasAttachment(texture))
                return MessageType::CantSampleAttachment;

            if (!texture || !texture->prepareSampledImage(context))
                return MessageType::SamplerNotSet;

            mUsedItems += texture->itemId();

            if (desc.descriptor_type
                == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
                setBindGroupResource(desc.set, isVariableLengthArray,
                    {
                        .binding = desc.binding,
                        .resource =
                            KDGpu::TextureViewBinding{
                                .textureView = texture->getView(),
                            },
                        .arrayElement = arrayElement,
                    });
            } else {
                setBindGroupResource(desc.set, isVariableLengthArray,
                    {
                        .binding = desc.binding,
                        .resource =
                            KDGpu::TextureViewSamplerBinding{
                                .textureView = texture->getView(),
                                .sampler = sampler,
                            },
                        .arrayElement = arrayElement,
                    });
            }
        }
        break;
    }

    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
        const auto imageBinding = find(mBindings.images, desc.name);
        if (!imageBinding)
            return MessageType::ImageNotSet;

        const auto texture = static_cast<VKTexture *>(imageBinding->texture);
        if (!texture || !texture->prepareStorageImage(context))
            return MessageType::ImageNotSet;
        texture->boundAsImage(imageBinding->level);

        mUsedItems += imageBinding->bindingItemId;
        mUsedItems += texture->itemId();

        setBindGroupResource(desc.set, isVariableLengthArray,
            {
                .binding = desc.binding,
                .resource =
                    KDGpu::ImageBinding{
                        .textureView = texture->getView(imageBinding->level,
                            imageBinding->layer,
                            toKDGpu(imageBinding->format)) },
                .arrayElement = arrayElement,
            });
        break;
    }

    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
        const auto imageBinding = find(mBindings.images, desc.name);
        if (!imageBinding)
            return MessageType::ImageNotSet;

        const auto texture = static_cast<VKTexture *>(imageBinding->texture);
        auto buffer = texture ? texture->textureBuffer() : nullptr;
        if (!buffer)
            return MessageType::ImageNotSet;

        const auto readable =
            !(desc.decoration_flags & SPV_REFLECT_DECORATION_NON_READABLE);
        const auto writeable =
            !(desc.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);
        buffer->prepareStorageTexelBuffer(context, readable, writeable);
        if (writeable)
            addWritableBuffer(buffer);

        mUsedItems += imageBinding->bindingItemId;
        mUsedItems += texture->itemId();
        setBindGroupResource(desc.set, isVariableLengthArray,
            {
                .binding = desc.binding,
                .resource =
                    KDGpu::StorageTexelBufferBinding{
                        .buffer = buffer->buffer(),
                        .format = toKDGpu(imageBinding->format),
                    },
                .arrayElement = arrayElement,
            });
        break;
    }

    case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {
        const auto binding = find(mBindings.accelerationStructures, desc.name);
        auto accelerationStructure = (binding
                ? static_cast<VKAccelerationStructure *>(
                      binding->accelerationStructure)
                : nullptr);
        if (!accelerationStructure)
            return MessageType::AccelerationStructureNotAssigned;

        accelerationStructure->prepare(context, scriptEngine);
        mUsedItems += binding->bindingItemId;
        mUsedItems += accelerationStructure->usedItems();
        const auto &topLevelAs = accelerationStructure->topLevelAs();
        if (!topLevelAs.isValid())
            return MessageType::AccelerationStructureNotAssigned;

        setBindGroupResource(desc.set, isVariableLengthArray,
            {
                .binding = desc.binding,
                .resource =
                    KDGpu::AccelerationStructureBinding{
                        .accelerationStructure = topLevelAs },
                .arrayElement = arrayElement,
            });
        break;
    }
    default:
        Q_ASSERT(!"descriptor type not handled");
        return MessageType::NotImplemented;
    }
    return MessageType::None;
}
