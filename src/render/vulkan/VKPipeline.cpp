
#include "VKPipeline.h"
#include "VKTarget.h"
#include "VKProgram.h"
#include "VKBuffer.h"
#include "VKTexture.h"
#include "VKStream.h"

namespace
{
    template<typename T>
    T* findByName(std::vector<T> &items, const auto &name) {
        const auto it = std::find_if(begin(items), end(items),
            [&](const auto &item) { return item.name == name; });
        return (it == items.end() ? nullptr : &*it);
    };

    Field::DataType getBufferMemberDataType(const QString &type)
    {
        if (type.startsWith("i"))
          return Field::DataType::Int32;
        if (type.startsWith("u"))
          return Field::DataType::Uint32;
        return Field::DataType::Float;
    }

    int getBufferMemberElementCount(const QString &type) 
    {
        if (type.endsWith("vec2")) return 2;
        if (type.endsWith("vec3")) return 3;
        if (type.endsWith("vec4")) return 4;
        if (type.endsWith("mat2"))   return 2*2;
        if (type.endsWith("mat2x2")) return 2*2;
        if (type.endsWith("mat2x3")) return 2*3;
        if (type.endsWith("mat2x4")) return 2*4;
        if (type.endsWith("mat3"))   return 3*3;
        if (type.endsWith("mat3x2")) return 3*2;
        if (type.endsWith("mat3x3")) return 3*3;
        if (type.endsWith("mat3x4")) return 3*4;
        if (type.endsWith("mat4"))   return 4*4;
        if (type.endsWith("mat4x2")) return 4*2;
        if (type.endsWith("mat4x3")) return 4*3;
        if (type.endsWith("mat4x4")) return 4*4;
        return 1;
    }

    int getBufferMemberDataSize(const QString &type) 
    {
        return 4 * getBufferMemberElementCount(type);
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

bool VKPipeline::applyPrintfBindings()
{
    // TODO:
    return true;
}

KDGpu::RenderPassCommandRecorder VKPipeline::beginRenderPass(VKContext &context)
{
    updateBindings(context);

    auto passOptions = mTarget->prepare(context);
    auto renderPass = context.commandRecorder->beginRenderPass(passOptions);
    renderPass.setPipeline(mGraphicsPipeline);

    for (auto i = 0u; i < mBindGroups.size(); ++i)
        if (mBindGroups[i].bindGroup.isValid())
            renderPass.setBindGroup(i, mBindGroups[i].bindGroup);
    
    if (mVertexStream) {
        const auto buffers = mVertexStream->getBuffers();
        for (auto i = 0u; i < buffers.size(); ++i)
            renderPass.setVertexBuffer(i, buffers[i]->getReadOnlyBuffer(context));
    }
    return renderPass;
}

KDGpu::ComputePassCommandRecorder VKPipeline::beginComputePass(VKContext &context)
{
    updateBindings(context);

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
        const auto it = std::find_if(interface.buffers.begin(), interface.buffers.end(),
            [](const auto &buffer) { return buffer.name == "gl_DefaultUniformBlock"; });
        if (it == interface.buffers.end())
            continue;
        const auto &members = it->members;

        auto &buffer = mDefaultUniformBlocks[stage];
        if (!buffer.isValid()) {
            auto size = 0;
            for (const auto &member : members)
                size = std::max(size, member.offset + getBufferMemberDataSize(member.type));
            
            buffer = context.device.createBuffer(KDGpu::BufferOptions{
                .size = static_cast<uint32_t>(size),
                .usage = KDGpu::BufferUsageFlagBits::UniformBufferBit,
                .memoryUsage = KDGpu::MemoryUsage::CpuToGpu
            });
        }

        auto bufferData = static_cast<std::byte*>(buffer.map());
        auto offset = 0;
        for (const auto &member : members) {  
            const auto it = std::find_if(begin(mUniformBindings), end(mUniformBindings),
                [&](const VKUniformBinding &binding) { return binding.name == member.name; });
            if (it != mUniformBindings.end()) {
                const auto &binding = *it;
                const auto type = getBufferMemberDataType(member.type);
                const auto count = getBufferMemberElementCount(member.type);
                switch (type) {
    #define ADD(DATATYPE, TYPE) \
                    case DATATYPE: { \
                        auto values = getValues<TYPE>(scriptEngine, \
                            binding.values, binding.bindingItemId, count, mMessages); \
                        std::memcpy(&bufferData[member.offset], values.data(), \
                            values.size() * sizeof(values[0])); \
                        break; \
                    }
                    ADD(Field::DataType::Int32, int32_t);
                    ADD(Field::DataType::Uint32, uint32_t);
                    ADD(Field::DataType::Float, float);
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
    for (const auto &[stage, interface] : mProgram.interface()) {
        for (const auto &buffer : interface.buffers)
            if (!createOrUpdateBindGroup(buffer.set, buffer.binding,
                KDGpu::ResourceBindingLayout{
                    .binding = buffer.binding,
                    .resourceType = (buffer.bindingType ==
                        spirvCross::Interface::BindingType::UniformBuffer ?
                        KDGpu::ResourceBindingType::UniformBuffer :
                        KDGpu::ResourceBindingType::StorageBuffer),
                    .shaderStages = stage
                }))
                return false;

        for (const auto &texture : interface.textures)
            if (!createOrUpdateBindGroup(texture.set, texture.binding,
                KDGpu::ResourceBindingLayout{
                    .binding = texture.binding,
                    .resourceType = KDGpu::ResourceBindingType::CombinedImageSampler,
                    .shaderStages = stage
                }))
                return false;

        for (const auto &image : interface.images)
            if (!createOrUpdateBindGroup(image.set, image.binding,
                KDGpu::ResourceBindingLayout{
                    .binding = image.binding,
                    .resourceType = KDGpu::ResourceBindingType::StorageImage,
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
    
    for (const auto &[stage, interface] : mProgram.interface()) {
        // buffer bindings
        for (const auto &buffer : interface.buffers) {
            if (buffer.name == "gl_DefaultUniformBlock") {
                Q_ASSERT(mDefaultUniformBlocks[stage].isValid());
                getBindGroup(buffer.set).resources.push_back({
                    .binding = buffer.binding,
                    .resource = KDGpu::UniformBufferBinding{ 
                        .buffer = mDefaultUniformBlocks[stage]
                    }
                });
            }
            else {
                const auto bufferBinding = findByName(mBufferBindings, buffer.name);
                if (!bufferBinding)
                    return false;

                if (buffer.bindingType == spirvCross::Interface::BindingType::UniformBuffer) {
                    getBindGroup(buffer.set).resources.push_back({
                        .binding = buffer.binding,
                        .resource = KDGpu::UniformBufferBinding{ 
                            .buffer = bufferBinding->buffer->getReadOnlyBuffer(context) 
                        }
                    });
                }
                else {
                    getBindGroup(buffer.set).resources.push_back({
                        .binding = buffer.binding,
                        .resource = KDGpu::StorageBufferBinding{ 
                            .buffer = bufferBinding->buffer->getReadOnlyBuffer(context) 
                        }
                    });
                }
            }
        }

        // sampler/texture bindings
        for (const auto &texture : interface.textures) {
            const auto samplerBinding = findByName(mSamplerBindings, texture.name);
            if (!samplerBinding || !samplerBinding->texture->prepareImageSampler(context))
                return false;

            // TODO: do not recreated every time
            const auto& sampler = mSamplers.emplace_back(
                context.device.createSampler(KDGpu::SamplerOptions{ 
                    .magFilter = KDGpu::FilterMode::Linear, 
                    .minFilter = KDGpu::FilterMode::Linear
                }));

            getBindGroup(texture.set).resources.push_back({
                .binding = texture.binding,
                .resource = KDGpu::TextureViewSamplerBinding{ 
                    .textureView = samplerBinding->texture->textureView(), 
                    .sampler = sampler
                }
            });
        }

        // image bindings
        for (const auto &image : interface.images) {
            const auto imageBinding = findByName(mImageBindings, image.name);
            if (!imageBinding || !imageBinding->texture->prepareStorageImage(context))
                return false;

            getBindGroup(image.set).resources.push_back({
                .binding = image.binding,
                .resource = KDGpu::ImageBinding{ 
                    .textureView = imageBinding->texture->textureView(),
                }
            });
        }
    }

    Q_ASSERT(mBindGroups.size() == mBindGroupLayouts.size());
    for (auto i = 0; i < mBindGroups.size(); ++i) {
        auto &bindGroup = mBindGroups[i];
        Q_ASSERT(!bindGroup.resources.empty());
        Q_ASSERT(bindGroup.resources.size() == bindGroup.bindings.size());
        bindGroup.bindGroup = context.device.createBindGroup({
            .layout = mBindGroupLayouts[i],
            .resources = bindGroup.resources
        });
    }
    return true;
}
