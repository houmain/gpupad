
#include "VKPipeline.h"
#include "VKTarget.h"
#include "VKProgram.h"
#include "VKBuffer.h"
#include "VKTexture.h"
#include "VKStream.h"

namespace
{
    const auto gl_DefaultUniformBlock = QStringLiteral("gl_DefaultUniformBlock");

    template<typename T>
    T* findByName(std::vector<T> &items, const auto &name) {
        const auto it = std::find_if(begin(items), end(items),
            [&](const auto &item) { return item.name == name; });
        return (it == items.end() ? nullptr : &*it);
    };

    Field::DataType getBufferMemberDataType(const SpvReflectBlockVariable &variable)
    {
        // TODO: complete
        switch (variable.type_description->op) {
            case SpvOpTypeInt: 
                return (variable.numeric.scalar.signedness ? 
                    Field::DataType::Int32 : Field::DataType::Uint32);

            case SpvOpTypeFloat:
            case SpvOpTypeVector:
            case SpvOpTypeMatrix:
                return (variable.numeric.scalar.width == 32 ?
                    Field::DataType::Float : Field::DataType::Double);
            default: break;
        }
        Q_ASSERT(!"variable type not handled");
        return Field::DataType::Float;
    }

    int getBufferMemberElementCount(const SpvReflectBlockVariable &variable) 
    {
        // TODO: complete
        switch (variable.type_description->op) {
            case SpvOpTypeVector: return variable.numeric.vector.component_count;
            case SpvOpTypeMatrix: return variable.numeric.matrix.row_count * 
                                         variable.numeric.matrix.column_count;
            default: break;
        }
        return 1;
    }

    KDGpu::ResourceBindingType getResourceType(SpvReflectDescriptorType type)
    {
        return static_cast<KDGpu::ResourceBindingType>(type);
    }
} // namespace

VKPipeline::VKPipeline(ItemId itemId, VKProgram *program, 
    VKTarget *target, VKStream *vertexStream)
    : mItemId(itemId)
    , mProgram(*program)
    , mTarget(target)
    , mVertexStream(vertexStream)
{
}

VKPipeline::~VKPipeline() = default;

bool VKPipeline::apply(const VKUniformBinding &binding)
{
    mUniformBindings.push_back(binding);
    return true;
}

bool VKPipeline::apply(const VKSamplerBinding &binding)
{
    mSamplerBindings.push_back(binding);
    return true;
}

bool VKPipeline::apply(const VKImageBinding &binding)
{
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

KDGpu::ComputePassCommandRecorder VKPipeline::beginComputePass(VKContext &context)
{
    auto computePass = context.commandRecorder->beginComputePass();
    computePass.setPipeline(mComputePipeline);

    for (auto i = 0u; i < mBindGroups.size(); ++i)
        if (mBindGroups[i].bindGroup.isValid())
            computePass.setBindGroup(i, mBindGroups[i].bindGroup);
    
    return computePass;
}

auto VKPipeline::getBindGroup(uint32_t set) -> BindGroup& {
    mBindGroups.resize(std::max(set + 1, 
        static_cast<uint32_t>(mBindGroups.size())));
    return mBindGroups[set];
}

bool VKPipeline::createOrUpdateBindGroup(
    uint32_t set, uint32_t binding,
    const KDGpu::ResourceBindingLayout &layout)
{
    auto &bindGroup = getBindGroup(set);
    auto &bindings = bindGroup.bindings;

    const auto it = std::find_if(begin(bindings), end(bindings),
        [&](const KDGpu::ResourceBindingLayout &layout) { 
            return layout.binding == binding; 
        });
    if (it != end(bindings)) {
        it->shaderStages |= layout.shaderStages;
        if (*it != layout) {
            mMessages += MessageList::insert(
                mProgram.itemId(), MessageType::IncompatibleBindings);
            return false;
        }
    }
    else {
        bindings.push_back(layout);
    }
    return true;
}

template <typename T>
std::vector<T> getValues(ScriptEngine &scriptEngine, 
    const QStringList &expressions, ItemId itemId, int count, 
    MessagePtrSet &messages)
{
    const auto values = scriptEngine.evaluateValues(expressions, itemId, messages); 
    if (count != values.count())
        if (count != 3 || values.count() != 4)
            messages += MessageList::insert(itemId,
                MessageType::UniformComponentMismatch,
                QString("(%1/%2)").arg(values.count()).arg(count));
    
    auto results = std::vector<T>();
    results.reserve(count);
    for (const auto &value : values)
        results.push_back(static_cast<T>(value));
    return results;
}

void VKPipeline::updateDefaultUniformBlock(VKContext &context, 
    ScriptEngine &scriptEngine)
{
    for (const auto &[stage, interface] : mProgram.interface()) {
        auto descriptor = std::add_pointer_t<SpvReflectDescriptorBinding>{ };
        for (auto i = 0u; i < interface->descriptor_binding_count; ++i)
            if (interface->descriptor_bindings[i].type_description->type_name == gl_DefaultUniformBlock) {
                descriptor = &interface->descriptor_bindings[i];
                break;
            }
        if (!descriptor)
            continue;

        auto &buffer = mDefaultUniformBlocks[stage];
        if (!buffer.isValid())
            buffer = context.device.createBuffer(KDGpu::BufferOptions{
                .size = descriptor->block.size,
                .usage = KDGpu::BufferUsageFlagBits::UniformBufferBit,
                .memoryUsage = KDGpu::MemoryUsage::CpuToGpu
            });

        auto bufferData = static_cast<std::byte*>(buffer.map());
        for (auto i = 0u; i < descriptor->block.member_count; ++i) {
            const auto& member = descriptor->block.members[i];
            const auto it = std::find_if(begin(mUniformBindings), end(mUniformBindings),
                [&](const VKUniformBinding &binding) { return binding.name == member.name; });
            if (it != mUniformBindings.end()) {
                const auto &binding = *it;
                const auto type = getBufferMemberDataType(member);
                const auto count = getBufferMemberElementCount(member);
                switch (type) {
#define ADD(DATATYPE, TYPE) \
                    case DATATYPE: { \
                        auto values = getValues<TYPE>(scriptEngine, \
                            binding.values, binding.bindingItemId, count, mMessages); \
                        std::memcpy(&bufferData[member.offset], values.data(), \
                            values.size() * sizeof(values[0])); \
                        break; \
                    }
                    ADD(Field::DataType::Int8, int8_t);
                    ADD(Field::DataType::Int16, int16_t);
                    ADD(Field::DataType::Int32, int32_t);
                    //ADD(Field::DataType::Int64, int64_t);
                    ADD(Field::DataType::Uint8, uint8_t);
                    ADD(Field::DataType::Uint16, uint16_t);
                    ADD(Field::DataType::Uint32, uint32_t);
                    //ADD(Field::DataType::Uint64, uint64_t);
                    ADD(Field::DataType::Float, float);
                    ADD(Field::DataType::Double, double);
#undef ADD
                }
                mUsedItems += binding.bindingItemId;
            }
        }
        buffer.unmap();
    }
}

bool VKPipeline::createGraphics(VKContext &context, KDGpu::PrimitiveOptions &primitiveOptions)
{
    if (std::exchange(mCreated, true))
        return mGraphicsPipeline.isValid();

    if (!createLayout(context))
        return false;

    mGraphicsPipeline = context.device.createGraphicsPipeline({
        .shaderStages = mProgram.getShaderStages(),
        .layout = mPipelineLayout,
        .vertex = (mVertexStream ? mVertexStream->getVertexOptions() : 
                   KDGpu::VertexOptions{ }),
        .renderTargets = mTarget->getRenderTargetOptions(),
        .depthStencil = mTarget->getDepthStencilOptions(),
        .primitive = primitiveOptions,
        .multisample = mTarget->getMultisampleOptions()
    });

    return mGraphicsPipeline.isValid();
}

bool VKPipeline::createCompute(VKContext &context)
{
    if (std::exchange(mCreated, true))
        return mComputePipeline.isValid();

    if (!createLayout(context))
        return false;

    const auto stages = mProgram.getShaderStages();
    if (stages.size() != 1 || stages[0].stage != KDGpu::ShaderStageFlagBits::ComputeBit)
        return false;

    mComputePipeline = context.device.createComputePipeline({
        .layout = mPipelineLayout,
        .shaderStage = {
            .shaderModule = stages[0].shaderModule,
            .entryPoint = stages[0].entryPoint
        },
    });
    return mComputePipeline.isValid();
}

bool VKPipeline::createLayout(VKContext &context)
{    
    for (const auto &[stage, interface] : mProgram.interface())
        for (auto i = 0u; i < interface->descriptor_binding_count; ++i) {
            const auto& descriptor = interface->descriptor_bindings[i];
            if (!createOrUpdateBindGroup(descriptor.set, descriptor.binding,
                KDGpu::ResourceBindingLayout{
                    .binding = descriptor.binding,
                    .resourceType = getResourceType(descriptor.descriptor_type),
                    .shaderStages = stage
                }))
                return false;
        }

    for (const auto &bindGroup : mBindGroups)
        mBindGroupLayouts.emplace_back(
            context.device.createBindGroupLayout({
                .bindings = bindGroup.bindings
            }));
    
    mPipelineLayout = context.device.createPipelineLayout({
        .bindGroupLayouts = { mBindGroupLayouts.begin(), mBindGroupLayouts.end() }
    });
    return true;
}

bool VKPipeline::updateBindings(VKContext &context)
{   
    mSamplers.clear();

    for (auto &bindGroup : mBindGroups) {
        bindGroup.resources = { };
        bindGroup.bindGroup = { };
    }

    for (const auto &[stage, interface] : mProgram.interface())
        for (auto i = 0u; i < interface->descriptor_binding_count; ++i) {
            const auto& desc = interface->descriptor_bindings[i];
            switch (desc.descriptor_type) {
                case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    if (desc.type_description->type_name == gl_DefaultUniformBlock) {
                        Q_ASSERT(mDefaultUniformBlocks[stage].isValid());
                        getBindGroup(desc.set).resources.push_back({
                            .binding = desc.binding,
                            .resource = KDGpu::UniformBufferBinding{ 
                                .buffer = mDefaultUniformBlocks[stage]
                            }
                        });
                    }
                    else {
                        const auto bufferBinding = findByName(mBufferBindings, 
                            desc.type_description->type_name);
                        if (!bufferBinding) {
                            mMessages += MessageList::insert(mItemId,
                                MessageType::BufferNotSet,
                                desc.type_description->type_name);
                            return false;
                        }

                        getBindGroup(desc.set).resources.push_back({
                            .binding = desc.binding,
                            .resource = KDGpu::UniformBufferBinding{ 
                                .buffer = bufferBinding->buffer->getReadOnlyBuffer(context) 
                            }
                        });
                    }
                    break;

                case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
                    if (desc.type_description->type_name == ShaderPrintf::bufferBindingName()) {
                        getBindGroup(desc.set).resources.push_back({
                            .binding = desc.binding,
                            .resource = KDGpu::StorageBufferBinding{ 
                                .buffer = mProgram.printf().getInitializedBuffer(context)
                            }
                        });
                    }
                    else {
                        const auto bufferBinding = findByName(mBufferBindings, 
                            desc.type_description->type_name);
                        if (!bufferBinding) {
                            mMessages += MessageList::insert(mItemId,
                                MessageType::BufferNotSet,
                                desc.type_description->type_name);
                            return false;
                        }

                        getBindGroup(desc.set).resources.push_back({
                            .binding = desc.binding,
                            .resource = KDGpu::StorageBufferBinding{ 
                                .buffer = bufferBinding->buffer->getReadWriteBuffer(context) 
                            }
                        });
                    }
                    break;
                }

                case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
                    const auto samplerBinding = findByName(mSamplerBindings, desc.name);
                    if (!samplerBinding || !samplerBinding->texture->prepareImageSampler(context)) {
                        mMessages += MessageList::insert(mItemId,
                            MessageType::UnformNotSet, desc.name);
                        return false;
                    }

                    // TODO: do not recreate every time
                    const auto& sampler = mSamplers.emplace_back(
                        context.device.createSampler(KDGpu::SamplerOptions{ 
                            .magFilter = KDGpu::FilterMode::Linear, 
                            .minFilter = KDGpu::FilterMode::Linear
                        }));

                    getBindGroup(desc.set).resources.push_back({
                        .binding = desc.binding,
                        .resource = KDGpu::TextureViewSamplerBinding{ 
                            .textureView = samplerBinding->texture->textureView(), 
                            .sampler = sampler
                        }
                    });
                    break;
                }

                case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
                    const auto imageBinding = findByName(mImageBindings, desc.name);
                    if (!imageBinding || !imageBinding->texture->prepareStorageImage(context)) {
                        mMessages += MessageList::insert(mItemId,
                            MessageType::UnformNotSet, desc.name);
                        return false;
                    }

                    getBindGroup(desc.set).resources.push_back({
                        .binding = desc.binding,
                        .resource = KDGpu::ImageBinding{ 
                            .textureView = imageBinding->texture->textureView(),
                        }
                    });
                    break;
                }

                default:
                    Q_ASSERT(!"descriptor type not handled");
                    break;
            }
        }

    Q_ASSERT(mBindGroups.size() == mBindGroupLayouts.size());
    for (auto i = 0u; i < mBindGroups.size(); ++i) {
        auto &bindGroup = mBindGroups[i];
        if (bindGroup.resources.empty())
            continue;

        Q_ASSERT(bindGroup.resources.size() == bindGroup.bindings.size());
        bindGroup.bindGroup = context.device.createBindGroup({
            .layout = mBindGroupLayouts[i],
            .resources = bindGroup.resources
        });
    }
    return true;
}
