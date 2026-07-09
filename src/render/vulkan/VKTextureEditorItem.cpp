#include "VKTextureEditorItem.h"
#include "VKTexture.h"
#include "VKWindow.h"
#include "render/ShaderCompiler.h"
#include <QMatrix4x4>
#include <QVector4D>

namespace {
    struct PipelineDesc
    {
        Texture::Target target{};
        Texture::Format textureFormat{};
        KDGpu::Format swapchainFormat{};
        bool picker{};

        friend bool operator<(const PipelineDesc &a, const PipelineDesc &b)
        {
            return std::tie(a.target, a.textureFormat, a.swapchainFormat,
                       a.picker)
                < std::tie(b.target, b.textureFormat, b.swapchainFormat,
                    b.picker);
        }
    };

    KDGpu::PushConstantRange pushConstantRange(size_t constantsSize)
    {
        return {
            .offset = 0,
            .size = static_cast<uint32_t>(constantsSize),
            .shaderStages = KDGpu::ShaderStageFlagBits::VertexBit
                | KDGpu::ShaderStageFlagBits::FragmentBit,
        };
    }

    struct Pipeline
    {
        Pipeline(KDGpu::Device &device, const PipelineDesc &pipelineDesc,
            const QString &vertexShaderSource,
            const QString &fragmentShaderSource, size_t constantsSize)
            : desc(pipelineDesc)
        {
            auto vertexSpirv = ShaderCompiler::compileSpirvVulkanGLSL(
                Shader::ShaderType::Vertex, vertexShaderSource);
            auto fragmentSpirv = ShaderCompiler::compileSpirvVulkanGLSL(
                Shader::ShaderType::Fragment, fragmentShaderSource);
            if (vertexSpirv.empty() || fragmentSpirv.empty())
                return;

            vertexShader = device.createShaderModule(vertexSpirv);
            fragmentShader = device.createShaderModule(fragmentSpirv);
            if (!vertexShader.isValid() || !fragmentShader.isValid())
                return;

            auto bindings = std::vector<KDGpu::ResourceBindingLayout>{ {
                .binding = 0,
                .resourceType =
                    KDGpu::ResourceBindingType::CombinedImageSampler,
                .shaderStages = KDGpu::ShaderStageFlagBits::FragmentBit,
            } };
            if (desc.picker) {
                bindings.push_back({
                    .binding = 1,
                    .resourceType = KDGpu::ResourceBindingType::StorageImage,
                    .shaderStages = KDGpu::ShaderStageFlagBits::FragmentBit,
                });
            }

            bindGroupLayout =
                device.createBindGroupLayout({ .bindings = bindings });
            if (!bindGroupLayout.isValid())
                return;

            pipelineLayout = device.createPipelineLayout({
                .bindGroupLayouts = { bindGroupLayout.handle() },
                .pushConstantRanges = { pushConstantRange(constantsSize) },
            });
            if (!pipelineLayout.isValid())
                return;

            pipeline = device.createGraphicsPipeline({
                .shaderStages = {
                    { .shaderModule = vertexShader.handle(),
                        .stage = KDGpu::ShaderStageFlagBits::VertexBit },
                    { .shaderModule = fragmentShader.handle(),
                        .stage = KDGpu::ShaderStageFlagBits::FragmentBit },
                },
                .layout = pipelineLayout.handle(),
                .renderTargets = { {
                    .format = desc.swapchainFormat,
                    .blending = {
                        .blendingEnabled = true,
                        .color = {
                            .srcFactor = KDGpu::BlendFactor::SrcAlpha,
                            .dstFactor =
                                KDGpu::BlendFactor::OneMinusSrcAlpha,
                        },
                        .alpha = {
                            .srcFactor = KDGpu::BlendFactor::SrcAlpha,
                            .dstFactor =
                                KDGpu::BlendFactor::OneMinusSrcAlpha,
                        },
                    },
                } },
                .depthStencil = {
                    .format = KDGpu::Format::UNDEFINED,
                    .depthTestEnabled = false,
                    .depthWritesEnabled = false,
                },
                .primitive = {
                    .topology = KDGpu::PrimitiveTopology::TriangleStrip,
                    .cullMode = KDGpu::CullModeFlagBits::None,
                },
            });
        }

        PipelineDesc desc;
        KDGpu::ShaderModule vertexShader;
        KDGpu::ShaderModule fragmentShader;
        KDGpu::BindGroupLayout bindGroupLayout;
        KDGpu::PipelineLayout pipelineLayout;
        KDGpu::GraphicsPipeline pipeline;
    };

    KDGpu::AddressMode toKDGpuAddressMode(TextureEditorItem::WrapMode wrapMode)
    {
        using WM = TextureEditorItem::WrapMode;
        switch (wrapMode) {
        case WM::ClampToBorder:  return KDGpu::AddressMode::ClampToBorder;
        case WM::ClampToEdge:    return KDGpu::AddressMode::ClampToEdge;
        case WM::Repeat:         return KDGpu::AddressMode::Repeat;
        case WM::MirroredRepeat: return KDGpu::AddressMode::MirroredRepeat;
        }
        return KDGpu::AddressMode::ClampToBorder;
    }
} // namespace

struct VKTextureEditorItem::PipelineCache
{
    Pipeline *getPipeline(KDGpu::Device &device, const PipelineDesc &desc,

        size_t constantsSize)
    {
        auto &pipeline = mPipelines[desc];
        if (!pipeline || !pipeline->pipeline.isValid()) {
            const auto shaderDesc = ShaderDesc{
                .target = desc.target,
                .format = desc.textureFormat,
                .picker = desc.picker,
            };
            pipeline = std::make_unique<Pipeline>(device, desc,
                vertexShaderSource, buildFragmentShader(shaderDesc),
                constantsSize);
            if (!pipeline->pipeline.isValid()) {
                mPipelines.erase(desc);
                return nullptr;
            }
        }
        return pipeline.get();
    }

    std::map<PipelineDesc, std::unique_ptr<Pipeline>> mPipelines;
};

struct VKTextureEditorItem::TextureBinding
{
    void reset(KDGpu::Queue &queue)
    {
        if (bindGroup.isValid() || sampler.isValid())
            queue.waitUntilIdle();

        bindGroup = {};
        sampler = {};
        bindGroupLayout = {};
        samplerLinear = false;
    }

    void release(KDGpu::Device &device, KDGpu::Queue &queue)
    {
        reset(queue);
        if (pickerTexture)
            pickerTexture->release(device);
        pickerTexture.reset();
        pickerColorWritten = false;
    }

    bool ensurePickerTexture()
    {
        if (!pickerTexture) {
            auto data = TextureData{};
            data.create(Texture::Target::Target1D, Texture::Format::RGBA32F, 1,
                1, 1, 1);
            pickerTexture = std::make_unique<VKTexture>(data, 1);
            pickerTexture->boundAsImage();
        }
        return (pickerTexture != nullptr);
    }

    bool ensureBindGroup(KDGpu::Device &device, KDGpu::Queue &queue,
        const Pipeline &pipeline, VKTexture &texture, bool linear,
        TextureEditorItem::WrapMode wrapMode)
    {
        if (bindGroup.isValid()
            && bindGroupLayout == pipeline.bindGroupLayout.handle()
            && samplerLinear == linear && mWrapMode == wrapMode)
            return true;

        if (bindGroup.isValid() || sampler.isValid())
            queue.waitUntilIdle();
        bindGroup = {};
        const auto addressMode = toKDGpuAddressMode(wrapMode);
        sampler = device.createSampler({
            .magFilter = linear ? KDGpu::FilterMode::Linear
                                : KDGpu::FilterMode::Nearest,
            .minFilter = linear ? KDGpu::FilterMode::Linear
                                : KDGpu::FilterMode::Nearest,
            .mipmapFilter = linear ? KDGpu::MipmapFilterMode::Linear
                                   : KDGpu::MipmapFilterMode::Nearest,
            .u = addressMode,
            .v = addressMode,
            .w = addressMode,
            .lodMinClamp = 0.0f,
            .lodMaxClamp =
                static_cast<float>(std::max(texture.levels(), 1) - 1),
        });
        if (!sampler.isValid())
            return false;

        auto &view = texture.getView();
        if (!view.isValid())
            return false;

        auto resources = std::vector<KDGpu::BindGroupEntry>{ {
            .binding = 0,
            .resource =
                KDGpu::TextureViewSamplerBinding{
                    .textureView = view.handle(),
                    .sampler = sampler.handle(),
                    .layout = texture.currentLayout(),
                },
        } };
        if (pipeline.desc.picker) {
            if (!pickerTexture)
                return false;
            auto &pickerView = pickerTexture->getView();
            if (!pickerView.isValid())
                return false;
            resources.push_back({
                .binding = 1,
                .resource =
                    KDGpu::ImageBinding{
                        .textureView = pickerView.handle(),
                        .layout = KDGpu::TextureLayout::General,
                    },
            });
        }

        bindGroup = device.createBindGroup({
            .layout = pipeline.bindGroupLayout.handle(),
            .resources = std::move(resources),
        });
        if (!bindGroup.isValid())
            return false;

        bindGroupLayout = pipeline.bindGroupLayout.handle();
        samplerLinear = linear;
        mWrapMode = wrapMode;
        return true;
    }

    KDGpu::Sampler sampler;
    KDGpu::BindGroup bindGroup;
    KDGpu::Handle<KDGpu::BindGroupLayout_t> bindGroupLayout;
    std::unique_ptr<VKTexture> pickerTexture;
    bool pickerColorWritten{};
    bool samplerLinear{};
    WrapMode mWrapMode{};
};

void VKTextureEditorItem::resetTextureBinding()
{
    if (mTextureBinding && window().initialized())
        mTextureBinding->reset(window().queue());
}

VKTextureEditorItem::VKTextureEditorItem(VKWindow *parent)
    : TextureEditorItem(parent)
{
}

VKTextureEditorItem::~VKTextureEditorItem()
{
    Q_ASSERT(!mTexture);
    Q_ASSERT(!mShare);
}

void VKTextureEditorItem::releaseGpu()
{
    if (!window().initialized())
        return;

    releaseGL();
    if (mTextureBinding)
        mTextureBinding->release(window().device(), window().queue());
    mTextureBinding.reset();
    if (mTexture)
        mTexture->release(window().device());
    releaseShareState();
    mTexture.reset();
    mPipelineCache.reset();
    mSharedTextureHandle = {};
    mTextureSamples = 1;
}

void VKTextureEditorItem::prepareGpu()
{
    if (!window().initialized())
        return;

    if (!mPickerEnabled) {
        Q_EMIT pickerColorChanged({});
        return;
    }

    if (!mTextureBinding)
        mTextureBinding = std::make_unique<TextureBinding>();
    if (!mTextureBinding->ensurePickerTexture())
        return;

    mTextureBinding->pickerColorWritten = false;

    auto context = makeContext();
    context.commandRecorder =
        context.device.createCommandRecorder({ .queue = context.queue });
    if (!mTextureBinding->pickerTexture->prepareStorageImage(context))
        return;
    submitCommandQueue(context);
}

void VKTextureEditorItem::paintGpu(const QSizeF &bounds, const QPointF &offset)
{
    if (!window().initialized() || mImage.isNull())
        return;

    if (mUpload && uploadTexture())
        mUpload = false;

    renderTexture(getTransform(bounds, offset));
}

void VKTextureEditorItem::submittedGpu()
{
    if (!mPickerEnabled || !mTextureBinding
        || !mTextureBinding->pickerColorWritten
        || !mTextureBinding->pickerTexture
        || !mTextureBinding->pickerTexture->texture().isValid())
        return;

    window().queue().waitUntilIdle();

    auto context = makeContext();
    context.commandRecorder =
        context.device.createCommandRecorder({ .queue = context.queue });
    mTextureBinding->pickerTexture->beginDownload(context);
    submitCommandQueue(context);
    if (!mTextureBinding->pickerTexture->finishDownload())
        return;

    auto pickerColor = QVector4D{};
    const auto *data = reinterpret_cast<const float *>(
        mTextureBinding->pickerTexture->data().getData());
    if (data)
        pickerColor = QVector4D{ data[0], data[1], data[2], data[3] };
    mTextureBinding->pickerColorWritten = false;

    Q_EMIT pickerColorChanged(pickerColor);
}

bool VKTextureEditorItem::downloadImage(TextureData *image)
{
    Q_ASSERT(image);
    if (!mTexture || !mTexture->texture().isValid())
        return TextureEditorItem::downloadImage(image);

    if (!window().initialized())
        return false;

    auto context = makeContext();
    context.commandRecorder =
        context.device.createCommandRecorder({ .queue = context.queue });
    mTexture->beginDownload(context);
    submitCommandQueue(context);

    if (!mTexture->finishDownload())
        return false;

    *image = mTexture->data();
    return true;
}

void VKTextureEditorItem::copySharedTexture(ShareHandle textureHandle,
    int samples)
{
    Q_ASSERT(textureHandle);
    if (!window().initialized() || !textureHandle.handle)
        return;

    const auto textureSamples = std::max(samples, 1);
    const auto typeChanged = (mSharedTextureHandle.type != textureHandle.type);
    if (!mSharedTextureHandle.sameResource(textureHandle)
        || mTextureSamples != textureSamples)
        resetTextureBinding();

    if (typeChanged || mUpload) {
        releaseGL();
        resetTextureBinding();
        if (mTexture)
            mTexture->release(window().device());
        releaseShareState();
        mTexture.reset();
        mUpload = false;
    }

    mSharedTextureHandle = std::move(textureHandle);
    mTextureSamples = textureSamples;

    if (mSharedTextureHandle.type == ShareHandleType::OPENGL_TEXTURE_ID) {
        copyGLTexture(mSharedTextureHandle);
    } else {
        copyImportedTexture(mSharedTextureHandle);
    }
}

VKWindow &VKTextureEditorItem::window()
{
    return *static_cast<VKWindow *>(parent());
}

VKContext VKTextureEditorItem::makeContext()
{
    return VKContext{
        .device = window().device(),
        .queue = window().queue(),
        .ktxDeviceInfo = window().ktxDeviceInfo(),
    };
}

void VKTextureEditorItem::submitCommandQueue(VKContext &context)
{
    if (context.commandRecorder) {
        context.commandBuffers.push_back(context.commandRecorder->finish());
        context.commandRecorder.reset();
    }

    if (!context.commandBuffers.empty()) {
        auto submitOptions = KDGpu::SubmitOptions{
            .commandBuffers =
                std::vector<KDGpu::RequiredHandle<KDGpu::CommandBuffer_t>>(
                    context.commandBuffers.begin(),
                    context.commandBuffers.end()),
        };
        context.queue.submit(submitOptions);
        context.queue.waitUntilIdle();
        context.commandBuffers.clear();
    }
    context.stagingBuffers.clear();
}

bool VKTextureEditorItem::uploadTexture()
{
    resetTextureBinding();
    if (mTexture)
        mTexture->release(window().device());
    mTexture = std::make_unique<VKTexture>(mImage, mTextureSamples);
    mTexture->boundAsSampler();

    auto context = makeContext();
    context.commandRecorder =
        context.device.createCommandRecorder({ .queue = context.queue });
    if (!mTexture->prepareSampledImage(context)) {
        mTexture->release(window().device());
        mTexture.reset();
        return false;
    }
    submitCommandQueue(context);
    return true;
}

bool VKTextureEditorItem::renderTexture(const QMatrix4x4 &transform)
{
    if (!mTexture)
        return false;

    if (!mPipelineCache)
        mPipelineCache = std::make_unique<PipelineCache>();

    auto &gpuWindow = window();
    const auto desc = PipelineDesc{
        .target = mTexture->target(),
        .textureFormat = mTexture->format(),
        .swapchainFormat = gpuWindow.swapchainFormat(),
        .picker = mPickerEnabled,
    };
    auto *pipeline =
        mPipelineCache->getPipeline(gpuWindow.device(), desc, sizeof(Params));
    if (!pipeline)
        return false;

    const auto linear = mMagnifyLinear && canLinearFilter(mImage.format())
        && mTexture->samples() == 1;
    if (!mTextureBinding)
        mTextureBinding = std::make_unique<TextureBinding>();
    if (!mTextureBinding->ensureBindGroup(gpuWindow.device(), gpuWindow.queue(),
            *pipeline, *mTexture, linear, static_cast<WrapMode>(mWrapMode)))
        return false;

    const auto constants = getParams(transform, mTexture->samples());

    auto &renderPass = gpuWindow.renderPass();
    renderPass.setPipeline(pipeline->pipeline);
    renderPass.setBindGroup(0, mTextureBinding->bindGroup,
        pipeline->pipelineLayout);
    renderPass.pushConstant(pushConstantRange(sizeof(Params)), &constants,
        pipeline->pipelineLayout);
    renderPass.draw({ .vertexCount = 4 });
    if (mPickerEnabled && mTextureBinding)
        mTextureBinding->pickerColorWritten = true;
    return true;
}
