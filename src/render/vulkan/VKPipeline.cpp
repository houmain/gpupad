
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

    template <typename C>
    auto find(C &container, const QString &name)
    {
        const auto it = container.find(name);
        return (it == container.end() ? nullptr : &it->second);
    }

    bool isBufferBinding(SpvReflectDescriptorType type)
    {
        return (type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER
            || type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }

    QString getBufferMemberFullName(const SpvReflectBlockVariable &block,
        uint32_t arrayElement, const SpvReflectBlockVariable &member)
    {
        if (isGlobalUniformBlockName(block.type_description->type_name))
            return member.name;

        if (block.type_description->op == SpvOpTypeArray)
            return QStringLiteral("%1[%2].%3")
                .arg(block.type_description->type_name)
                .arg(arrayElement)
                .arg(member.name);

        return QStringLiteral("%1.%2").arg(block.type_description->type_name,
            member.name);
    }

    QStringView getBaseName(QStringView name)
    {
        if (!name.endsWith(']'))
            return name;
        if (auto bracket = name.lastIndexOf('['))
            return getBaseName(name.left(bracket));
        return {};
    }

    std::vector<int> getArrayIndices(QStringView name)
    {
        auto result = std::vector<int>();
        while (name.endsWith(']')) {
            const auto pos = name.lastIndexOf('[');
            if (pos < 0)
                break;
            const auto index = name.mid(pos + 1, name.size() - pos - 2).toInt();
            result.insert(result.begin(), index);
            name = name.left(pos);
        }
        return result;
    }

    std::span<const uint32_t> getBufferMemberArrayDims(
        const SpvReflectBlockVariable &variable)
    {
        const auto &type_desc = *variable.type_description;
        if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY)
            return { variable.array.dims, variable.array.dims_count };
        return {};
    }

    template <typename T>
    void copyBufferMember(const SpvReflectBlockVariable &member,
        std::byte *dest, int elementOffset, int elementCount, const T *source)
    {
        const auto arraySize = getBufferMemberArraySize(member);
        const auto columns = getBufferMemberColumnCount(member);
        const auto rows = getBufferMemberRowCount(member);
        const auto arrayStride = getBufferMemberArrayStride(member);
        const auto columnStride = getBufferMemberColumnStride(member);
        const auto elementSize = sizeof(T) * rows;

        for (auto a = 0; a < arraySize; ++a) {
            const auto arrayBegin = dest;
            if (elementOffset-- <= 0 && elementCount-- > 0) {
                for (auto c = 0; c < columns; ++c) {
                    std::memcpy(dest, source, elementSize);
                    source += rows;
                    dest += (columnStride ? columnStride : elementSize);
                }
            }
            if (arrayStride)
                dest = arrayBegin + arrayStride;
        }
    }

    KDGpu::ResourceBindingType getResourceType(SpvReflectDescriptorType type)
    {
        return static_cast<KDGpu::ResourceBindingType>(type);
    }

    KDGpu::SamplerOptions getSamplerOptions(const VKSamplerBinding &binding,
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
            return KDGpu::AddressMode{};
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

    template <typename F>
    void forEachBufferMemberRec(const QString &name,
        const SpvReflectBlockVariable &member, int memberOffset,
        const F &function)
    {
        memberOffset += member.offset;

        if (!member.member_count)
            return function(name, member, memberOffset);

        if (auto size = getBufferMemberArraySize(member)) {
            const auto arrayStride = getBufferMemberArrayStride(member);
            for (auto a = 0; a < size; ++a) {
                const auto element = QString("[%1]").arg(a);
                for (auto i = 0u; i < member.member_count; ++i)
                    forEachBufferMemberRec(
                        name + element + '.' + member.members[i].name,
                        member.members[i], memberOffset + a * arrayStride,
                        function);
            }
        } else {
            for (auto i = 0u; i < member.member_count; ++i)
                forEachBufferMemberRec(name + '.' + member.members[i].name,
                    member.members[i], memberOffset, function);
        }
    }

    template <typename F>
    void forEachArrayElementRec(SpvReflectDescriptorBinding desc,
        uint32_t arrayDim, uint32_t &arrayElement, const F &function,
        bool *variableLengthArrayDonePtr = nullptr)
    {
        if (arrayDim >= desc.array.dims_count)
            return function(desc, arrayElement++, variableLengthArrayDonePtr);

        // update descriptor to contain array index in the names
        const auto baseName = desc.name;
        const auto baseTypeName = (desc.type_description->type_name
                ? desc.type_description->type_name
                : "");
        auto typeDesc = *desc.type_description;
        desc.type_description = &typeDesc;

        const auto count = desc.array.dims[arrayDim];
        const auto variableLengthArray = (count == 0);
        auto variableLengthArrayDone = false;

        for (auto i = 0u; variableLengthArray || i < count; ++i) {
            const auto formatArray = [](std::string baseName, uint32_t i) {
                return baseName + "[" + std::to_string(i) + "]";
            };
            const auto name = formatArray(baseName, i);
            const auto typeName = formatArray(baseTypeName, i);
            desc.name = name.c_str();
            typeDesc.type_name = typeName.c_str();
            forEachArrayElementRec(desc, arrayDim + 1, arrayElement, function,
                (variableLengthArray ? &variableLengthArrayDone : nullptr));
            if (variableLengthArrayDone)
                break;
        }
    }

    uint32_t getMaxBindingNumberInSet(const Spirv::Interface &interface,
        uint32_t s)
    {
        auto maxBindingNumber = 0u;
        for (auto i = 0u; i < interface->descriptor_binding_count; ++i) {
            const auto &desc = interface->descriptor_bindings[i];
            maxBindingNumber = std::max(maxBindingNumber, desc.binding);
        }
        return maxBindingNumber;
    }
} // namespace

VKPipeline::VKPipeline(ItemId itemId, VKProgram *program)
    : mItemId(itemId)
    , mProgram(*program)
{
}

VKPipeline::~VKPipeline() = default;

KDGpu::RenderPassCommandRecorder VKPipeline::beginRenderPass(VKContext &context,
    bool flipViewport)
{
    if (mVertexStream)
        for (auto &buffer : mVertexStream->getBuffers())
            buffer->prepareVertexBuffer(context);

    auto passOptions = mTarget->prepare(context);
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
    const VKSamplerBinding &samplerBinding)
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
            mMessages += MessageList::insert(mProgram.itemId(),
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
                mMessages += MessageList::insert(mProgram.itemId(),
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

void VKPipeline::applyBufferMemberBinding(std::span<std::byte> bufferData,
    const SpvReflectBlockVariable &member, const VKUniformBinding &binding,
    int memberOffset, int elementOffset, int elementCount,
    ScriptEngine &scriptEngine)
{
    const auto type = getBufferMemberDataType(member);
    const auto memberData = &bufferData[memberOffset];
    const auto valuesPerElement = getBufferMemberColumnCount(member)
        * getBufferMemberRowCount(member);
    switch (type) {
#define ADD(DATATYPE, TYPE)                                                  \
    case DATATYPE: {                                                         \
        const auto values = getValues<TYPE>(scriptEngine, binding.values, 0, \
            elementCount * valuesPerElement, binding.bindingItemId);         \
        copyBufferMember<TYPE>(member, memberData, elementOffset,            \
            elementCount, values.data());                                    \
        break;                                                               \
    }
        ADD(Field::DataType::Int32, int32_t);
        ADD(Field::DataType::Uint32, uint32_t);
        ADD(Field::DataType::Int64, int64_t);
        ADD(Field::DataType::Uint64, uint64_t);
        ADD(Field::DataType::Float, float);
        ADD(Field::DataType::Double, double);
#undef ADD
    default: Q_ASSERT(!"not handled data type");
    }
}

bool VKPipeline::applyBufferMemberBindings(std::span<std::byte> bufferData,
    const QString &name, const SpvReflectBlockVariable &member,
    int memberOffset, ScriptEngine &scriptEngine)
{
    auto bindingSet = false;
    for (const auto &[bindingName, binding] : mBindings.uniforms) {
        if (bindingName == name) {
            applyBufferMemberBinding(bufferData, member, binding, memberOffset,
                0, getBufferMemberArraySize(member), scriptEngine);
            mUsedItems += binding.bindingItemId;
            bindingSet = true;
            continue;
        }

        // compare array elements also by basename
        const auto baseName = getBaseName(bindingName);
        if (baseName == name) {
            const auto indices = getArrayIndices(bindingName);
            Q_ASSERT(!indices.empty());
            const auto dims = getBufferMemberArrayDims(member);
            if (indices.size() > dims.size())
                continue;

            auto elementOffset = 0;
            for (auto i = 0u; i < indices.size(); ++i)
                elementOffset += indices[i]
                    * (i == dims.size() - 1 ? 1 : dims[i]);

            auto elementCount = 1;
            for (auto i = indices.size(); i < dims.size(); ++i)
                elementCount *= dims[i];

            applyBufferMemberBinding(bufferData, member, binding, memberOffset,
                elementOffset, elementCount, scriptEngine);
            mUsedItems += binding.bindingItemId;
            bindingSet = true;
        }
    }
    return bindingSet;
}

bool VKPipeline::applyBufferMemberBindings(std::span<std::byte> bufferData,
    const SpvReflectBlockVariable &block, uint32_t arrayElement,
    ScriptEngine &scriptEngine)
{
    auto memberUsed = false;
    auto memberSet = false;
    for (auto i = 0u; i < block.member_count; ++i) {
        const auto &member = block.members[i];
        if (member.flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED)
            continue;
        memberUsed = true;

        const auto name = getBufferMemberFullName(block, arrayElement, member);
        forEachBufferMemberRec(name, member, 0,
            [&](const QString &name, const auto &member, int memberOffset) {
                if (applyBufferMemberBindings(bufferData, name, member,
                        memberOffset, scriptEngine)) {
                    memberSet = true;
                } else {
                    mMessages += MessageList::insert(mItemId,
                        MessageType::UniformNotSet, name);
                }
            });
    }
    return (!memberUsed || memberSet
        || isGlobalUniformBlockName(block.type_description->type_name));
}

bool VKPipeline::updateDynamicBufferBindings(VKContext &context,
    const SpvReflectDescriptorBinding &desc, uint32_t arrayElement,
    ScriptEngine &scriptEngine)
{
    auto &block = getDynamicUniformBuffer(desc.set, desc.binding, arrayElement);
    if (!block.buffer.isValid()) {
        block.buffer = context.device.createBuffer(KDGpu::BufferOptions{
            .size = desc.block.size,
            .usage = KDGpu::BufferUsageFlagBits::UniformBufferBit,
            .memoryUsage = KDGpu::MemoryUsage::CpuToGpu,
        });
        block.size = desc.block.size;
    }
    Q_ASSERT(block.buffer.isValid());
    Q_ASSERT(desc.block.size == block.size);
    if (!block.buffer.isValid() || desc.block.size != block.size)
        return false;

    auto bufferData = std::span<std::byte>(
        static_cast<std::byte *>(block.buffer.map()), block.size);
    const auto guard = qScopeGuard([&] { block.buffer.unmap(); });

    if (!applyBufferMemberBindings(bufferData, desc.block, arrayElement,
            scriptEngine))
        return false;

    setBindGroupResource(desc.set, false,
        {
            .binding = desc.binding,
            .resource = KDGpu::UniformBufferBinding{ .buffer = block.buffer },
            .arrayElement = arrayElement,
        });

    return true;
}

bool VKPipeline::hasPushConstants() const
{
    return (mPushConstantRange.size > 0);
}

bool VKPipeline::updatePushConstants(ScriptEngine &scriptEngine)
{
    for (const auto &[stage, interface] : mProgram.interface())
        for (auto i = 0u; i < interface->push_constant_block_count; ++i) {
            const auto &block = interface->push_constant_blocks[i];
            if (const auto bufferBinding = find(mBindings.buffers,
                    block.type_description->type_name)) {
                if (!bufferBinding->buffer) {
                    mMessages += MessageList::insert(mItemId,
                        MessageType::BufferNotSet,
                        block.type_description->type_name);
                    return false;
                }

                auto &buffer = *bufferBinding->buffer;
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

    if (!createLayout(context))
        return false;

    auto vertexOptions = KDGpu::VertexOptions{};
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
        mMessages +=
            MessageList::insert(mItemId, MessageType::CreatingPipelineFailed);

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
    VKAccelerationStructure *accelStruct)
{
    if (std::exchange(mCreated, true))
        return mRayTracingPipeline.isValid();

    if (!createLayout(context))
        return false;

    mAccelerationStructure = accelStruct;

    // https://www.willusher.io/graphics/2019/11/20/the-sbt-three-ways/
    auto options = KDGpu::RayTracingPipelineOptions{};
    options.shaderStages = mProgram.getShaderStages();
    options.layout = mPipelineLayout;

    auto shaderGroupEntriesRayGen = std::vector<size_t>();
    auto shaderGroupEntriesHit = std::vector<std::pair<size_t, uint32_t>>();
    auto shaderGroupEntriesMiss = std::vector<std::pair<size_t, uint32_t>>();
    for (const auto &shader : mProgram.shaders()) {
        switch (shader.type()) {
        case Shader::ShaderType::RayGeneration:
            shaderGroupEntriesRayGen.push_back(options.shaderGroups.size());
            options.shaderGroups.push_back({
                .type = KDGpu::RayTracingShaderGroupType::General,
                .generalShaderIndex = shader.shaderIndex(),
            });
            break;

        case Shader::ShaderType::RayMiss:
            shaderGroupEntriesMiss.emplace_back(options.shaderGroups.size(), 0);
            options.shaderGroups.push_back({
                .type = KDGpu::RayTracingShaderGroupType::General,
                .generalShaderIndex = shader.shaderIndex(),
            });
            break;

        case Shader::ShaderType::RayIntersection:
        case Shader::ShaderType::RayAnyHit:
        case Shader::ShaderType::RayClosestHit:   {
            // merge in last hit group (entry is currently unused)
            if (shaderGroupEntriesHit.empty()
                || shaderGroupEntriesHit.back().first + 1
                    != options.shaderGroups.size()) {
                shaderGroupEntriesHit.emplace_back(options.shaderGroups.size(),
                    0);
                options.shaderGroups.push_back({
                    .type = KDGpu::RayTracingShaderGroupType::TrianglesHit,
                });
            }
            auto &shaderGroup = options.shaderGroups.back();
            if (shader.type() == Shader::ShaderType::RayIntersection) {
                shaderGroup.type =
                    KDGpu::RayTracingShaderGroupType::ProceduralHit;
                shaderGroup.intersectionShaderIndex = shader.shaderIndex();
            } else if (shader.type() == Shader::ShaderType::RayAnyHit) {
                shaderGroup.anyHitShaderIndex = shader.shaderIndex();
            } else if (shader.type() == Shader::ShaderType::RayClosestHit) {
                shaderGroup.closestHitShaderIndex = shader.shaderIndex();
            }
            break;
        }

        default:
            mMessages += MessageList::insert(mItemId, MessageType::ShaderError,
                "shader type not implemented");
            return false;
        }
    }

    mRayTracingPipeline = context.device.createRayTracingPipeline(options);
    if (!mRayTracingPipeline.isValid())
        return false;

    auto &sbt = mRayTracingShaderBindingTable;
    sbt = KDGpu::RayTracingShaderBindingTable(&context.device,
        KDGpu::RayTracingShaderBindingTableOptions{
            .nbrMissShaders =
                static_cast<uint32_t>(shaderGroupEntriesMiss.size()),
            .nbrHitShaders =
                static_cast<uint32_t>(shaderGroupEntriesHit.size()),
        });
    for (auto shaderGroupIndex : shaderGroupEntriesRayGen)
        sbt.addRayGenShaderGroup(mRayTracingPipeline, shaderGroupIndex);
    for (auto [shaderGroupIndex, entry] : shaderGroupEntriesHit)
        sbt.addHitShaderGroup(mRayTracingPipeline, shaderGroupIndex, entry);
    for (auto [shaderGroupIndex, entry] : shaderGroupEntriesMiss)
        sbt.addMissShaderGroup(mRayTracingPipeline, shaderGroupIndex, entry);
    return true;
}

bool VKPipeline::createLayout(VKContext &context)
{
    mPushConstantRange = KDGpu::PushConstantRange{};
    for (const auto &[stage, interface] : mProgram.interface()) {
        for (auto i = 0u; i < interface->descriptor_binding_count; ++i) {
            const auto &desc = interface->descriptor_bindings[i];
            if (!desc.accessed)
                continue;

            auto count = getBindingArraySize(desc.array);
            auto flags = KDGpu::ResourceBindingFlagBits::None;
            if (count == 0) {
                if (desc.binding
                    != getMaxBindingNumberInSet(interface, desc.set)) {
                    mMessages += MessageList::insert(mItemId,
                        MessageType::OnlyLastBindingMayBeUnsizedArray);
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

        for (auto i = 0u; i < interface->push_constant_block_count; ++i) {
            const auto &block = interface->push_constant_blocks[i];
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
        mMessages += MessageList::insert(mItemId,
            MessageType::MaxPushConstantSizeExceeded,
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

void VKPipeline::setBindings(VKBindings &&bindings)
{
    mBindings = std::move(bindings);
}

bool VKPipeline::updateBindings(VKContext &context, ScriptEngine &scriptEngine)
{
    for (auto &bindGroup : mBindGroups) {
        bindGroup.resources = {};
        bindGroup.bindGroup = {};
    }

    auto canRender = true;
    for (const auto &[stage, interface] : mProgram.interface())
        for (auto i = 0u; i < interface->descriptor_binding_count; ++i) {
            const auto &desc = interface->descriptor_bindings[i];
            if (!desc.accessed)
                continue;

            auto arrayElement = uint32_t{};
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
                        mMessages +=
                            MessageList::insert(mItemId, message, name);
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
            mMessages += MessageList::insert(mItemId,
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
    const auto getBufferBindingOffsetSize =
        [&](const VKBufferBinding &binding) {
            const auto &buffer = *binding.buffer;
            const auto offset = scriptEngine.evaluateUInt(binding.offset,
                buffer.itemId());
            const auto rowCount = scriptEngine.evaluateUInt(binding.rowCount,
                buffer.itemId());
            const auto size = (binding.stride ? rowCount * binding.stride
                                              : buffer.size() - offset);
            Q_ASSERT(size > 0
                && offset + size <= static_cast<size_t>(buffer.size()));
            return std::pair(offset, size);
        };

    switch (desc.descriptor_type) {
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        if (const auto bufferBinding =
                find(mBindings.buffers, desc.type_description->type_name)) {
            if (!bufferBinding->buffer)
                return MessageType::BufferNotSet;
            auto &buffer = *bufferBinding->buffer;
            mUsedItems += bufferBinding->bindingItemId;
            mUsedItems += bufferBinding->blockItemId;
            mUsedItems += buffer.usedItems();

            buffer.prepareUniformBuffer(context);

            const auto [offset, size] =
                getBufferBindingOffsetSize(*bufferBinding);
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
            if (!updateDynamicBufferBindings(context, desc, arrayElement,
                    scriptEngine))
                return MessageType::BufferNotSet;
        }
        break;

    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
        if (desc.type_description->type_name
            == ShaderPrintf::bufferBindingName()) {
            setBindGroupResource(desc.set, isVariableLengthArray,
                {
                    .binding = desc.binding,
                    .resource =
                        KDGpu::StorageBufferBinding{
                            .buffer =
                                mProgram.printf().getInitializedBuffer(context),
                        },
                });
        } else {
            const auto bufferBinding =
                find(mBindings.buffers, desc.type_description->type_name);
            if (!bufferBinding || !bufferBinding->buffer)
                return MessageType::BufferNotSet;
            auto &buffer = *bufferBinding->buffer;
            mUsedItems += bufferBinding->bindingItemId;
            mUsedItems += buffer.usedItems();

            const auto readable =
                !(desc.decoration_flags & SPV_REFLECT_DECORATION_NON_READABLE);
            const auto writeable =
                !(desc.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);
            buffer.prepareShaderStorageBuffer(context, readable, writeable);

            const auto [offset, size] =
                getBufferBindingOffsetSize(*bufferBinding);
            setBindGroupResource(desc.set, isVariableLengthArray,
                {
                    .binding = desc.binding,
                    .resource =
                        KDGpu::StorageBufferBinding{
                            .buffer = buffer.buffer(),
                            .offset = offset,
                            .size = size,
                        },
                    .arrayElement = arrayElement,
                });
        }
        break;
    }

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
            if (mTarget && mTarget->hasAttachment(samplerBinding->texture))
                return MessageType::CantSampleAttachment;

            if (!samplerBinding->texture
                || !samplerBinding->texture->prepareSampledImage(context))
                return MessageType::SamplerNotSet;

            mUsedItems += samplerBinding->texture->itemId();

            if (desc.descriptor_type
                == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
                setBindGroupResource(desc.set, isVariableLengthArray,
                    {
                        .binding = desc.binding,
                        .resource =
                            KDGpu::TextureViewBinding{
                                .textureView =
                                    samplerBinding->texture->getView(),
                            },
                        .arrayElement = arrayElement,
                    });
            } else {
                setBindGroupResource(desc.set, isVariableLengthArray,
                    {
                        .binding = desc.binding,
                        .resource =
                            KDGpu::TextureViewSamplerBinding{
                                .textureView =
                                    samplerBinding->texture->getView(),
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
        if (!imageBinding || !imageBinding->texture
            || !imageBinding->texture->prepareStorageImage(context))
            return MessageType::ImageNotSet;

        mUsedItems += imageBinding->bindingItemId;
        mUsedItems += imageBinding->texture->itemId();

        setBindGroupResource(desc.set, isVariableLengthArray,
            {
                .binding = desc.binding,
                .resource =
                    KDGpu::ImageBinding{
                        .textureView = imageBinding->texture->getView(
                            imageBinding->level, imageBinding->layer,
                            toKDGpu(imageBinding->format)) },
                .arrayElement = arrayElement,
            });
        break;
    }

    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return MessageType::TextureBuffersNotAvailable;

    case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {
        if (!mAccelerationStructure)
            return MessageType::AccelerationStructureNotAssigned;

        mAccelerationStructure->prepare(context, scriptEngine);
        mUsedItems += mAccelerationStructure->usedItems();
        const auto &topLevelAs = mAccelerationStructure->topLevelAs();
        if (!topLevelAs.isValid())
            return MessageType::AccelerationStructureNotAssigned;

        setBindGroupResource(desc.set, isVariableLengthArray,
            {
                .binding = desc.binding,
                .resource =
                    KDGpu::AccelerationStructureBinding{
                        .accelerationStructure = topLevelAs },
            });
        break;
    }
    default: Q_ASSERT(!"descriptor type not handled"); break;
    }
    return MessageType::None;
}
