#include "VKTextureEditorItem.h"
#include "VKTexture.h"
#include "render/vulkan/VKWindow.h"
#include "render/ShaderCompiler.h"
#include <KDGpu/bind_group.h>
#include <KDGpu/bind_group_layout_options.h>
#include <KDGpu/bind_group_options.h>
#include <KDGpu/command_recorder.h>
#include <KDGpu/device.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/pipeline_layout_options.h>
#include <KDGpu/render_pass_command_recorder.h>
#include <KDGpu/sampler.h>
#include <KDGpu/sampler_options.h>
#include <map>

namespace {
    struct PipelineDesc
    {
        QOpenGLTexture::Target target{};
        QOpenGLTexture::TextureFormat textureFormat{};
        KDGpu::Format swapchainFormat{};
        bool picker{};
        bool histogram{};

        friend bool operator<(const PipelineDesc &a, const PipelineDesc &b)
        {
            return std::tie(a.target, a.textureFormat, a.swapchainFormat,
                       a.picker, a.histogram)
                < std::tie(b.target, b.textureFormat, b.swapchainFormat,
                    b.picker, b.histogram);
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

            bindGroupLayout = device.createBindGroupLayout({
                .bindings = { {
                    .binding = 0,
                    .resourceType =
                        KDGpu::ResourceBindingType::CombinedImageSampler,
                    .shaderStages = KDGpu::ShaderStageFlagBits::FragmentBit,
                } },
            });
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
} // namespace

struct VKTextureEditorItem::PipelineCache
{
    Pipeline *getPipeline(KDGpu::Device &device, const PipelineDesc &desc,
        const QString &vertexShaderSource, const QString &fragmentShaderSource,
        size_t constantsSize)
    {
        auto &pipeline = mPipelines[desc];
        if (!pipeline || !pipeline->pipeline.isValid()) {
            pipeline = std::make_unique<Pipeline>(device, desc,
                vertexShaderSource, fragmentShaderSource, constantsSize);
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
    bool ensureBindGroup(KDGpu::Device &device, const Pipeline &pipeline,
        VKTexture &texture, bool linear)
    {
        if (bindGroup.isValid()
            && bindGroupLayout == pipeline.bindGroupLayout.handle()
            && samplerLinear == linear)
            return true;

        bindGroup = {};
        sampler = device.createSampler({
            .magFilter = linear ? KDGpu::FilterMode::Linear
                                : KDGpu::FilterMode::Nearest,
            .minFilter = linear ? KDGpu::FilterMode::Linear
                                : KDGpu::FilterMode::Nearest,
            .mipmapFilter = linear ? KDGpu::MipmapFilterMode::Linear
                                   : KDGpu::MipmapFilterMode::Nearest,
            .u = KDGpu::AddressMode::ClampToEdge,
            .v = KDGpu::AddressMode::ClampToEdge,
            .w = KDGpu::AddressMode::ClampToEdge,
            .lodMinClamp = 0.0f,
            .lodMaxClamp =
                static_cast<float>(std::max(texture.levels(), 1) - 1),
        });
        if (!sampler.isValid())
            return false;

        auto &view = texture.getView();
        if (!view.isValid())
            return false;

        bindGroup = device.createBindGroup({
            .layout = pipeline.bindGroupLayout.handle(),
            .resources = { {
                .binding = 0,
                .resource =
                    KDGpu::TextureViewSamplerBinding{
                        .textureView = view.handle(),
                        .sampler = sampler.handle(),
                        .layout = texture.currentLayout(),
                    },
            } },
        });
        if (!bindGroup.isValid())
            return false;

        bindGroupLayout = pipeline.bindGroupLayout.handle();
        samplerLinear = linear;
        return true;
    }

    KDGpu::Sampler sampler;
    KDGpu::BindGroup bindGroup;
    KDGpu::Handle<KDGpu::BindGroupLayout_t> bindGroupLayout;
    bool samplerLinear{};
};

void VKTextureEditorItem::resetTextureBinding()
{
    mTextureBinding.reset();
}

VKTextureEditorItem::VKTextureEditorItem(VKWindow *parent)
    : TextureEditorItem(parent)
{
}

VKTextureEditorItem::~VKTextureEditorItem()
{
    releaseGpu();
}

void VKTextureEditorItem::releaseGpu()
{
    releaseGL();
    if (mTexture && window().initialized())
        mTexture->release(window().device());
    mTextureBinding.reset();
    mTexture.reset();
    mShare.reset();
    mPipelineCache.reset();
    mShareSync.reset();
    mPreviewTextureHandle = {};
    mTextureSamples = 1;
}

void VKTextureEditorItem::paintGpu(const QMatrix4x4 &transform)
{
    if (updateTexture())
        renderTexture(transform);
}

bool VKTextureEditorItem::downloadImage(TextureData *image)
{
    Q_ASSERT(image);
    if (!mTexture || !mTexture->texture().isValid())
        return TextureEditorItem::downloadImage(image);

    if (!window().initialized())
        window().update();
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

void VKTextureEditorItem::setPreviewTexture(ShareSyncPtr shareSync,
    ShareHandle textureHandle, int samples)
{
    const auto previewSamples = std::max(samples, 1);
    const auto typeChanged = (mPreviewTextureHandle.type != textureHandle.type);
    if (mPreviewTextureHandle != textureHandle
        || mTextureSamples != previewSamples)
        mTextureBinding.reset();

    if (typeChanged) {
        releaseGL();
        if (mTexture && window().initialized())
            mTexture->release(window().device());
        mTexture.reset();
        mShare.reset();
    }

    mShareSync = std::move(shareSync);
    mPreviewTextureHandle = textureHandle;
    mTextureSamples = previewSamples;
    render();
}

VKWindow &VKTextureEditorItem::window()
{
    return static_cast<VKWindow &>(TextureEditorItem::window());
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
        context.queue.submit({
            .commandBuffers =
                std::vector<KDGpu::Handle<KDGpu::CommandBuffer_t>>(
                    context.commandBuffers.begin(),
                    context.commandBuffers.end()),
        });
        context.queue.waitUntilIdle();
        context.commandBuffers.clear();
    }
    context.stagingBuffers.clear();
}

bool VKTextureEditorItem::updateTexture()
{
    if (mImage.isNull() || !window().initialized())
        return false;

    if (mPreviewTextureHandle.type == ShareHandleType::OPENGL_TEXTURE_ID)
        return updateOpenGLTexture();

    if (mPreviewTextureHandle)
        return updateImportedTexture();

    if (mUpload || !mTexture || mTexture->samples() != mTextureSamples) {
        if (mTexture)
            mTexture->release(window().device());
        mTextureBinding.reset();
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
        mUpload = false;
    }

    return (mTexture && mTexture->texture().isValid());
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
        .picker = false,
        .histogram = false,
    };
    const auto shaderDesc = ShaderDesc{
        .target = desc.target,
        .format = desc.textureFormat,
        .picker = desc.picker,
        .histogram = desc.histogram,
    };
    auto *pipeline = mPipelineCache->getPipeline(gpuWindow.device(), desc,
        vertexShaderSource, buildFragmentShader(shaderDesc), sizeof(Params));
    if (!pipeline)
        return false;

    const auto linear = mMagnifyLinear && canLinearFilter(mImage.format())
        && mTexture->samples() == 1;
    if (!mTextureBinding)
        mTextureBinding = std::make_unique<TextureBinding>();
    if (!mTextureBinding->ensureBindGroup(gpuWindow.device(), *pipeline,
            *mTexture, linear))
        return false;

    const auto constants = getParams(transform, mTexture->samples());

    auto &renderPass = gpuWindow.renderPass();
    renderPass.setPipeline(pipeline->pipeline);
    renderPass.setBindGroup(0, mTextureBinding->bindGroup,
        pipeline->pipelineLayout);
    renderPass.pushConstant(pushConstantRange(sizeof(Params)), &constants,
        pipeline->pipelineLayout);
    renderPass.draw({ .vertexCount = 4 });
    return true;
}
