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
    void release(VKContext &context)
    {
        if (bindGroup.isValid() || sampler.isValid())
            context.queue.waitUntilIdle();
        bindGroup = {};
        sampler = {};
        bindGroupLayout = {};
        if (pickerTexture)
            pickerTexture->release(context.device);
        pickerTexture.reset();
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

    bool ensureBindGroup(VKContext &context, const Pipeline &pipeline,
        VKTexture &texture, bool linear, TextureEditorItem::WrapMode wrapMode)
    {
        if (bindGroup.isValid()
            && bindGroupLayout == pipeline.bindGroupLayout.handle()
            && samplerLinear == linear && mWrapMode == wrapMode)
            return true;

        if (bindGroup.isValid() || sampler.isValid())
            context.queue.waitUntilIdle();
        bindGroup = {};
        const auto addressMode = toKDGpuAddressMode(wrapMode);
        sampler = context.device.createSampler({
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

        bindGroup = context.device.createBindGroup({
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
    bool samplerLinear{};
    WrapMode mWrapMode{};
};

VKTextureEditorItem::VKTextureEditorItem(VKWindow *parent)
    : TextureEditorItem(parent, TextureData::RowOrder::TopToBottom)
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

    auto &context = window().context();
    releaseTextureSharing(context);
    mPipelineCache.reset();
}

void VKTextureEditorItem::prepareGpu()
{
    if (!window().initialized())
        return;

    auto &context = window().context();
    if (mTexture && !mTexture->prepareSampledImage(context))
        return;

    if (mPickerEnabled) {
        if (!mTextureBinding)
            mTextureBinding = std::make_unique<TextureBinding>();
        if (!mTextureBinding->ensurePickerTexture())
            return;

        mTextureBinding->pickerTexture->prepareStorageImage(context);
    }
}

void VKTextureEditorItem::paintGpu(const QSizeF &bounds, const QPointF &offset,
    const TextureData &image)
{
    if (!window().initialized() || image.isNull() || !mTexture)
        return;

    renderTexture(getTransform(bounds, offset), image);
}

void VKTextureEditorItem::releaseTextureSharing(VKContext &context)
{
    if (mTextureBinding)
        mTextureBinding->release(context);
    mTextureBinding.reset();
    if (mShare)
        mShare->release(context.device);
    mShare.reset();
#if defined(OPENGL_ENABLED)
    mGLState.reset();
#endif
    if (mTexture)
        mTexture->release(context.device);
    mTexture.reset();
    mCurrentShareHandle.reset();
    mTextureSamples = 1;
}

void VKTextureEditorItem::submittedGpu()
{
    if (mPickerEnabled && mTextureBinding->pickerTexture) {
        auto deviceLock = window().beginCommandQueue();
        auto &context = window().context();
        mTextureBinding->pickerTexture->beginDownload(context);
        window().submitCommandQueueWaitIdle();
        if (!mTextureBinding->pickerTexture->finishDownload())
            return;

        const auto *data = reinterpret_cast<const float *>(
            mTextureBinding->pickerTexture->data().getData());
        Q_EMIT pickerColorChanged(
            QVector4D{ data[0], data[1], data[2], data[3] });
    }
}

bool VKTextureEditorItem::downloadImage(TextureData *image)
{
    Q_ASSERT(image);
    if (!mTexture || !mTexture->texture().isValid())
        return false;

    if (!window().initialized())
        return false;

    auto deviceLock = window().beginCommandQueue();
    auto &context = window().context();

    mTexture->beginDownload(context);
    window().submitCommandQueueWaitIdle();
    if (!mTexture->finishDownload())
        return false;

    *image = mTexture->data();
    return true;
}

bool VKTextureEditorItem::copySharedTexture(ShareHandle shareHandle,
    int samples, const TextureData &image)
{
    if (!window().initialized())
        return false;
    auto deviceLock = window().beginCommandQueue();
    auto &context = window().context();

    const auto shareHandleData = shareHandle.lock();
    if (shareHandleData != mCurrentShareHandle.lock())
        releaseTextureSharing(context);

    if (!shareHandleData)
        return false;

    mCurrentShareHandle = shareHandle;
    mTextureSamples = std::max(samples, 1);

    if (shareHandleData->type == ShareHandleType::VK_TEXTURE_PTR) {
        auto &vkTexture = *static_cast<VKTexture *>(shareHandleData->handle);
        return copyVKTexture(context, vkTexture);
    }

    if (shareHandleData->type == ShareHandleType::OPENGL_TEXTURE_ID) {
#if defined(OPENGL_ENABLED)
        return copyGLTexture(context, *shareHandleData, image);
#else
        return false;
#endif
    }
    return copySharedTexture(context, *shareHandleData, image);
}

bool VKTextureEditorItem::copyVKTexture(VKContext &context, VKTexture &source)
{
    // check if texture handle is still valid for currently selected adapter
    const auto &rm = *context.device.graphicsApi()->resourceManager();
    if (!rm.getTexture(source.texture()))
        return false;

    if (!mTexture) {
        mTexture = std::make_unique<VKTexture>(source.data(), mTextureSamples);
        mTexture->boundAsSampler();
    }

    if (!mTexture->copy(context, source))
        return false;

    return true;
}

VKWindow &VKTextureEditorItem::window()
{
    return *static_cast<VKWindow *>(parent());
}

bool VKTextureEditorItem::uploadImage(const TextureData &image)
{
    Q_ASSERT(window().initialized() && !image.isNull());
    auto deviceLock = window().beginCommandQueue();
    auto &context = window().context();

    auto texture = std::make_unique<VKTexture>(image, 1);
    texture->boundAsSampler();

    releaseTextureSharing(context);
    mTexture = std::move(texture);
    return true;
}

bool VKTextureEditorItem::renderTexture(const QMatrix4x4 &transform,
    const TextureData &image)
{
    if (!mTexture)
        return false;

    auto &context = window().context();

    if (!mPipelineCache)
        mPipelineCache = std::make_unique<PipelineCache>();

    const auto desc = PipelineDesc{
        .target = mTexture->target(),
        .textureFormat = mTexture->format(),
        .swapchainFormat = window().swapchainFormat(),
        .picker = mPickerEnabled,
    };
    auto *pipeline =
        mPipelineCache->getPipeline(context.device, desc, sizeof(Params));
    if (!pipeline)
        return false;

    if (!mTextureBinding)
        mTextureBinding = std::make_unique<TextureBinding>();
    if (!mTextureBinding->ensureBindGroup(context, *pipeline, *mTexture,
            mMagnifyLinear, static_cast<WrapMode>(mWrapMode)))
        return false;

    const auto constants = getParams(transform, mTexture->samples(),
        image.depth(), image.rowOrder());

    auto &renderPass = window().renderPass();
    renderPass.setPipeline(pipeline->pipeline);
    renderPass.setBindGroup(0, mTextureBinding->bindGroup,
        pipeline->pipelineLayout);
    renderPass.pushConstant(pushConstantRange(sizeof(Params)), &constants,
        pipeline->pipelineLayout);
    renderPass.draw({ .vertexCount = 4 });
    return true;
}
