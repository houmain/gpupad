
#include "VKPipeline.h"
#include "VKBuffer.h"
#include "VKProgram.h"
#include "VKStream.h"
#include "VKTarget.h"
#include "VKTexture.h"
#include <QScopeGuard>

namespace {
    bool isDefaultUniformBlock(const char *name)
    {
        return (name == QStringLiteral("gl_DefaultUniformBlock")
            || name == QStringLiteral("$Global"));
    }

    template <typename T>
    T *findByName(std::vector<T> &items, const auto &name)
    {
        const auto it = std::find_if(begin(items), end(items),
            [&](const auto &item) { return item.name == name; });
        return (it == items.end() ? nullptr : &*it);
    };

    QString getBufferMemberFullName(const SpvReflectBlockVariable &block,
        const SpvReflectBlockVariable &member)
    {
        if (isDefaultUniformBlock(block.type_description->type_name))
            return member.name;
        return QStringLiteral("%1.%2")
            .arg(block.type_description->type_name)
            .arg(member.name);
    }

    Field::DataType getBufferMemberDataType(
        const SpvReflectBlockVariable &variable)
    {
        const auto &type_desc = *variable.type_description;
        if (type_desc.traits.numeric.scalar.width == 32) {
            if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT)
                return Field::DataType::Float;
            return (type_desc.traits.numeric.scalar.signedness
                    ? Field::DataType::Int32
                    : Field::DataType::Uint32);
        } else if (type_desc.traits.numeric.scalar.width == 64) {
            if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT)
                return Field::DataType::Double;
        }
        Q_ASSERT(!"variable type not handled");
        return Field::DataType::Float;
    }

    int getBufferMemberColumnCount(const SpvReflectBlockVariable &variable)
    {
        const auto &type_desc = *variable.type_description;
        if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX)
            return variable.numeric.matrix.column_count;
        if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR)
            return variable.numeric.vector.component_count;
        return 1;
    }

    int getBufferMemberRowCount(const SpvReflectBlockVariable &variable)
    {
        const auto &type_desc = *variable.type_description;
        if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX)
            return variable.numeric.matrix.row_count;
        return 1;
    }

    int getBufferMemberColumnStride(const SpvReflectBlockVariable &variable)
    {
        const auto &type_desc = *variable.type_description;
        if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX)
            return variable.numeric.matrix.stride;
        return 0;
    }

    int getBufferMemberArraySize(const SpvReflectBlockVariable &variable)
    {
        const auto &type_desc = *variable.type_description;
        auto count = 1u;
        if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY)
            for (auto i = 0u; i < variable.array.dims_count; ++i)
                count *= variable.array.dims[i];
        return static_cast<int>(count);
    }

    int getBufferMemberArrayStride(const SpvReflectBlockVariable &variable)
    {
        const auto &type_desc = *variable.type_description;
        if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY)
            return variable.array.stride;
        return 0;
    }

    int getBufferMemberElementCount(const SpvReflectBlockVariable &variable)
    {
        return getBufferMemberArraySize(variable)
            * getBufferMemberColumnCount(variable)
            * getBufferMemberRowCount(variable);
    }

    template <typename T>
    void copyBufferMember(const SpvReflectBlockVariable &member,
        std::byte *dest, const T *source)
    {
        const auto arraySize = getBufferMemberArraySize(member);
        const auto columns = getBufferMemberColumnCount(member);
        const auto rows = getBufferMemberRowCount(member);
        const auto arrayStride = getBufferMemberArrayStride(member);
        const auto columnStride = getBufferMemberColumnStride(member);
        const auto sourceStride = rows * sizeof(T);
        const auto destStride = (columnStride ? columnStride : sourceStride);
        const auto arrayPadding =
            (arrayStride ? arrayStride - (columns * destStride) : 0);

        for (auto a = 0; a < arraySize; ++a, dest += arrayPadding)
            for (auto c = 0; c < columns; ++c) {
                std::memcpy(dest, source, sourceStride);
                dest += destStride;
                source += rows;
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
} // namespace

VKPipeline::VKPipeline(ItemId itemId, VKProgram *program, VKTarget *target,
    VKStream *vertexStream)
    : mItemId(itemId)
    , mProgram(*program)
    , mTarget(target)
    , mVertexStream(vertexStream)
{
}

VKPipeline::~VKPipeline() = default;

void VKPipeline::clearBindings()
{
    mUniformBindings.clear();
    mBufferBindings.clear();
    mSamplerBindings.clear();
    mImageBindings.clear();
    mSamplers.clear();
}

bool VKPipeline::apply(const VKUniformBinding &binding)
{
    mUniformBindings.push_back(binding);
    return true;
}

bool VKPipeline::apply(const VKSamplerBinding &binding)
{
    // texture is allowed to be empty
    mSamplerBindings.push_back(binding);
    return true;
}

bool VKPipeline::apply(const VKImageBinding &binding)
{
    if (!binding.texture)
        return false;

    mImageBindings.push_back(binding);
    return true;
}

bool VKPipeline::apply(const VKBufferBinding &binding)
{
    if (!binding.buffer)
        return false;

    mBufferBindings.push_back(binding);
    return true;
}

KDGpu::RenderPassCommandRecorder VKPipeline::beginRenderPass(VKContext &context)
{
    auto passOptions = mTarget->prepare(context);
    auto renderPass = context.commandRecorder->beginRenderPass(passOptions);
    renderPass.setPipeline(mGraphicsPipeline);

    for (auto i = 0u; i < mBindGroups.size(); ++i)
        if (mBindGroups[i].bindGroup.isValid())
            renderPass.setBindGroup(i, mBindGroups[i].bindGroup);

    if (mVertexStream) {
        const auto &buffers = mVertexStream->getBuffers();
        const auto &offsets = mVertexStream->getBufferOffsets();
        for (auto i = 0u; i < buffers.size(); ++i)
            renderPass.setVertexBuffer(i,
                buffers[i]->getReadOnlyBuffer(context),
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
                MessageType::IncompatibleBindings);
            return false;
        }
        it->shaderStages |= layout.shaderStages;
    } else {
        bindings.push_back(layout);
    }
    return true;
}

void VKPipeline::setBindGroupResource(uint32_t set,
    KDGpu::BindGroupEntry &&resource)
{
    auto &bindGroup = getBindGroup(set);
    for (const auto &res : bindGroup.resources)
        if (res.binding == resource.binding) {
            // TODO: improve check
            if (res.resource.type() != resource.resource.type())
                mMessages += MessageList::insert(mProgram.itemId(),
                    MessageType::IncompatibleBindings);

            return;
        }
    bindGroup.resources.emplace_back(std::move(resource));
}

auto VKPipeline::getDefaultUniformBlock(uint32_t set,
    uint32_t binding) -> DefaultUniformBlock &
{
    auto it = std::find_if(mDefaultUniformBlocks.begin(),
        mDefaultUniformBlocks.end(), [&](const auto &block) {
            return (block->set == set && block->binding == binding);
        });
    if (it == mDefaultUniformBlocks.end()) {
        it = mDefaultUniformBlocks.emplace(mDefaultUniformBlocks.end(),
            new DefaultUniformBlock{
                .set = set,
                .binding = binding,
            });
    }
    return **it;
}

void VKPipeline::updateUniformBlockData(std::byte *bufferData,
    const SpvReflectBlockVariable &block, ScriptEngine &scriptEngine)
{
    for (auto i = 0u; i < block.member_count; ++i) {
        const auto &member = block.members[i];
        const auto fullName = getBufferMemberFullName(block, member);
        const auto it = std::find_if(begin(mUniformBindings),
            end(mUniformBindings), [&](const VKUniformBinding &binding) {
                return binding.name == fullName;
            });
        if (it == mUniformBindings.end()) {
            if ((member.flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED) == 0)
                mMessages += MessageList::insert(mItemId,
                    MessageType::UniformNotSet, fullName);
            continue;
        }

        const auto &binding = *it;
        const auto type = getBufferMemberDataType(member);
        const auto count = getBufferMemberElementCount(member);
        const auto memberData = &bufferData[member.offset];
        switch (type) {
#define ADD(DATATYPE, TYPE)                                               \
    case DATATYPE: {                                                      \
        const auto values = getValues<TYPE>(scriptEngine, binding.values, \
            binding.bindingItemId, count, mMessages);                     \
        copyBufferMember<TYPE>(member, memberData, values.data());        \
        break;                                                            \
    }
            ADD(Field::DataType::Int32, int32_t);
            ADD(Field::DataType::Uint32, uint32_t);
            ADD(Field::DataType::Float, float);
            ADD(Field::DataType::Double, double);
#undef ADD
        default: Q_ASSERT(!"not handled data type");
        }
        mUsedItems += binding.bindingItemId;
    }
}

void VKPipeline::updatePushConstants(
    KDGpu::RenderPassCommandRecorder &renderPass, ScriptEngine &scriptEngine)
{
    if (mPushConstantRange.size == 0)
        return;

    for (const auto &[stage, interface] : mProgram.interface()) {
        for (auto i = 0u; i < interface->push_constant_block_count; ++i) {
            auto &block = interface->push_constant_blocks[i];
            updateUniformBlockData(mPushConstantData.data(), block,
                scriptEngine);
        }
    }
    renderPass.pushConstant(mPushConstantRange, mPushConstantData.data());
}

void VKPipeline::updateDefaultUniformBlock(VKContext &context,
    ScriptEngine &scriptEngine)
{
    for (const auto &[stage, interface] : mProgram.interface()) {
        auto descriptor =
            std::add_pointer_t<const SpvReflectDescriptorBinding>{};
        for (auto i = 0u; i < interface->descriptor_binding_count; ++i)
            if (isDefaultUniformBlock(interface->descriptor_bindings[i]
                                          .type_description->type_name)) {
                descriptor = &interface->descriptor_bindings[i];
                break;
            }
        if (!descriptor || !descriptor->accessed)
            continue;

        auto &block =
            getDefaultUniformBlock(descriptor->set, descriptor->binding);
        if (!block.buffer.isValid()) {
            block.buffer = context.device.createBuffer(KDGpu::BufferOptions{
                .size = descriptor->block.size,
                .usage = KDGpu::BufferUsageFlagBits::UniformBufferBit,
                .memoryUsage = KDGpu::MemoryUsage::CpuToGpu,
            });
            block.size = descriptor->block.size;
        }

        Q_ASSERT(descriptor->block.size == block.size);
        auto bufferData = static_cast<std::byte *>(block.buffer.map());
        const auto guard = qScopeGuard([&] { block.buffer.unmap(); });

        updateUniformBlockData(bufferData, descriptor->block, scriptEngine);
    }
}

bool VKPipeline::createGraphics(VKContext &context,
    KDGpu::PrimitiveOptions &primitiveOptions)
{
    if (std::exchange(mCreated, true))
        return mGraphicsPipeline.isValid();

    if (!createLayout(context))
        return false;

    mGraphicsPipeline = context.device.createGraphicsPipeline({
        .shaderStages = mProgram.getShaderStages(),
        .layout = mPipelineLayout,
        .vertex = (mVertexStream ? mVertexStream->getVertexOptions()
                                 : KDGpu::VertexOptions{}),
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

bool VKPipeline::createLayout(VKContext &context)
{
    mPushConstantRange = KDGpu::PushConstantRange{};
    for (const auto &[stage, interface] : mProgram.interface()) {
        for (auto i = 0u; i < interface->descriptor_binding_count; ++i) {
            const auto &descriptor = interface->descriptor_bindings[i];
            if (!descriptor.accessed)
                continue;

            if (!createOrUpdateBindGroup(descriptor.set, descriptor.binding,
                    KDGpu::ResourceBindingLayout{
                        .binding = descriptor.binding,
                        .resourceType =
                            getResourceType(descriptor.descriptor_type),
                        .shaderStages = stage,
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

bool VKPipeline::updateBindings(VKContext &context)
{
    mSamplers.clear();

    for (auto &bindGroup : mBindGroups) {
        bindGroup.resources = {};
        bindGroup.bindGroup = {};
    }

    auto allBound = true;
    const auto notBoundError = [&](MessageType message, QString name) {
        mMessages += MessageList::insert(mItemId, message, name);
        allBound = false;
    };

    for (const auto &[stage, interface] : mProgram.interface())
        for (auto i = 0u; i < interface->descriptor_binding_count; ++i) {
            const auto &desc = interface->descriptor_bindings[i];
            if (!desc.accessed)
                continue;

            switch (desc.descriptor_type) {
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                if (isDefaultUniformBlock(desc.type_description->type_name)) {
                    auto &block =
                        getDefaultUniformBlock(desc.set, desc.binding);
                    Q_ASSERT(block.buffer.isValid());
                    if (block.buffer.isValid())
                        setBindGroupResource(desc.set,
                            {
                                .binding = desc.binding,
                                .resource =
                                    KDGpu::UniformBufferBinding{
                                        .buffer = block.buffer },
                            });
                } else {
                    const auto bufferBinding = findByName(mBufferBindings,
                        desc.type_description->type_name);
                    if (!bufferBinding) {
                        notBoundError(MessageType::BufferNotSet,
                            desc.type_description->type_name);
                        continue;
                    }

                    setBindGroupResource(desc.set,
                        {
                            .binding = desc.binding,
                            .resource =
                                KDGpu::UniformBufferBinding{
                                    .buffer =
                                        bufferBinding->buffer
                                            ->getReadOnlyBuffer(context) },
                        });
                }
                break;

            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
                if (desc.type_description->type_name
                    == ShaderPrintf::bufferBindingName()) {
                    setBindGroupResource(desc.set,
                        {
                            .binding = desc.binding,
                            .resource =
                                KDGpu::StorageBufferBinding{
                                    .buffer =
                                        mProgram.printf().getInitializedBuffer(
                                            context) },
                        });
                } else {
                    const auto bufferBinding = findByName(mBufferBindings,
                        desc.type_description->type_name);
                    if (!bufferBinding) {
                        notBoundError(MessageType::BufferNotSet,
                            desc.type_description->type_name);
                        continue;
                    }

                    setBindGroupResource(desc.set,
                        {
                            .binding = desc.binding,
                            .resource =
                                KDGpu::StorageBufferBinding{
                                    .buffer =
                                        bufferBinding->buffer
                                            ->getReadWriteBuffer(context) },
                        });
                }
                break;
            }

            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
                const auto samplerBinding =
                    findByName(mSamplerBindings, desc.name);
                if (!samplerBinding) {
                    if (desc.accessed)
                        notBoundError(MessageType::SamplerNotSet, desc.name);
                    continue;
                }

                // TODO: do not recreate every time
                const auto maxSamplerAnisotropy =
                    context.device.adapter()
                        ->properties()
                        .limits.maxSamplerAnisotropy;
                const auto &sampler = mSamplers.emplace_back(
                    context.device.createSampler(getSamplerOptions(
                        *samplerBinding, maxSamplerAnisotropy)));

                if (desc.descriptor_type
                    == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER) {
                    setBindGroupResource(desc.set,
                        {
                            .binding = desc.binding,
                            .resource =
                                KDGpu::SamplerBinding{ .sampler = sampler },
                        });
                } else {
                    if (mTarget->hasAttachment(samplerBinding->texture)) {
                        notBoundError(MessageType::CantSampleAttachment,
                            desc.name);
                        continue;
                    }

                    if (!samplerBinding->texture
                        || !samplerBinding->texture->prepareSampledImage(
                            context)) {
                        notBoundError(MessageType::SamplerNotSet, desc.name);
                        continue;
                    }

                    if (desc.descriptor_type
                        == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
                        setBindGroupResource(desc.set,
                            {
                                .binding = desc.binding,
                                .resource =
                                    KDGpu::TextureViewBinding{
                                        .textureView =
                                            samplerBinding->texture->getView(),
                                    },
                            });
                    } else {
                        setBindGroupResource(desc.set,
                            {
                                .binding = desc.binding,
                                .resource =
                                    KDGpu::TextureViewSamplerBinding{
                                        .textureView =
                                            samplerBinding->texture->getView(),
                                        .sampler = sampler,
                                    },
                            });
                    }
                }
                break;
            }

            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
                const auto imageBinding = findByName(mImageBindings, desc.name);
                if (!imageBinding
                    || !imageBinding->texture->prepareStorageImage(context)) {
                    if (desc.accessed)
                        notBoundError(MessageType::ImageNotSet, desc.name);
                    continue;
                }

                setBindGroupResource(desc.set,
                    {
                        .binding = desc.binding,
                        .resource =
                            KDGpu::ImageBinding{
                                .textureView = imageBinding->texture->getView(
                                    imageBinding->level, imageBinding->layer,
                                    toKDGpu(imageBinding->format)),
                            },
                    });
                break;
            }

            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                Q_ASSERT(!"Texture buffers are not yet available in KDGpu");
                break;

            default: Q_ASSERT(!"descriptor type not handled"); break;
            }
        }

    if (!allBound)
        return false;

    Q_ASSERT(mBindGroups.size() == mBindGroupLayouts.size());
    for (auto i = 0u; i < mBindGroups.size(); ++i) {
        auto &bindGroup = mBindGroups[i];
        if (bindGroup.resources.empty())
            continue;

        Q_ASSERT(bindGroup.resources.size() == bindGroup.bindings.size());
        bindGroup.bindGroup = context.device.createBindGroup({
            .layout = mBindGroupLayouts[i],
            .resources = bindGroup.resources,
        });
    }
    return true;
}
