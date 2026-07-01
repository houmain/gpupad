#include "VKTextureEditorBackground.h"
#include "VKWindow.h"
#include "render/ShaderCompiler.h"

struct VKTextureEditorBackground::Pipeline
{
    Pipeline(KDGpu::Device &device, KDGpu::Format format) : format(format)
    {
        vertexShader =
            device.createShaderModule(ShaderCompiler::compileSpirvVulkanGLSL(
                Shader::ShaderType::Vertex, vertexShaderSource));
        fragmentShader =
            device.createShaderModule(ShaderCompiler::compileSpirvVulkanGLSL(
                Shader::ShaderType::Fragment, fragmentShaderSource));

        pipelineLayout = device.createPipelineLayout({
            .pushConstantRanges = { {
                .size = sizeof(TextureEditorBackground::Params),
                .shaderStages = KDGpu::ShaderStageFlagBits::FragmentBit,
            } },
        });
        pipeline = device.createGraphicsPipeline({
            .shaderStages = {
                { .shaderModule = vertexShader.handle(),
                    .stage = KDGpu::ShaderStageFlagBits::VertexBit },
                { .shaderModule = fragmentShader.handle(),
                    .stage = KDGpu::ShaderStageFlagBits::FragmentBit },
            },
            .layout = pipelineLayout.handle(),
            .renderTargets = { { .format = format } },
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

    KDGpu::Format format{};
    KDGpu::ShaderModule vertexShader;
    KDGpu::ShaderModule fragmentShader;
    KDGpu::PipelineLayout pipelineLayout;
    KDGpu::GraphicsPipeline pipeline;
};

VKTextureEditorBackground::VKTextureEditorBackground(VKWindow *window)
    : TextureEditorBackground(window)
    , mWindow(*window)
{
}

VKTextureEditorBackground::~VKTextureEditorBackground() = default;

void VKTextureEditorBackground::releaseGpu()
{
    mPipeline.reset();
}

void VKTextureEditorBackground::paintGpu(const QSizeF &size,
    const QPointF &offset)
{
    const auto format = mWindow.swapchainFormat();
    if (!mPipeline || mPipeline->format != format)
        mPipeline = std::make_unique<Pipeline>(mWindow.device(), format);
    if (!mPipeline->pipeline.isValid())
        return;

    auto &renderPass = mWindow.renderPass();
    renderPass.setPipeline(mPipeline->pipeline);

    const auto params = getParams(size, offset);
    const auto constantsRange = KDGpu::PushConstantRange{
        .size = sizeof(TextureEditorBackground::Params),
        .shaderStages = KDGpu::ShaderStageFlagBits::FragmentBit,
    };
    renderPass.pushConstant(constantsRange, &params, mPipeline->pipelineLayout);
    renderPass.draw({ .vertexCount = 4 });
}
