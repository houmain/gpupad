#include "VKShader.h"

namespace {
    KDGpu::ShaderStageFlagBits getStageFlags(Shader::ShaderType type)
    {
        using ST = Shader::ShaderType;
        using KD = KDGpu::ShaderStageFlagBits;
        switch (type) {
        case ST::Vertex:                 return KD::VertexBit;
        case ST::Fragment:               return KD::FragmentBit;
        case ST::Geometry:               return KD::GeometryBit;
        case ST::TessellationControl:    return KD::TessellationControlBit;
        case ST::TessellationEvaluation: return KD::TessellationEvaluationBit;
        case ST::Compute:                return KD::ComputeBit;
        case ST::Includable:             break;
        }
        Q_UNREACHABLE();
        return {};
    }
} // namespace

VKShader::VKShader(Shader::ShaderType type,
    const QList<const Shader *> &shaders, const Session &session)
    : ShaderBase(type, shaders, session)
{
}

bool VKShader::compile(KDGpu::Device &device, ShaderPrintf &printf,
    int shiftBindingsInSet0)
{
    if (mInterface)
        return mShaderModule.isValid();

    auto spirv = generateSpirv(printf, shiftBindingsInSet0);
    mInterface = spirv.getInterface();
    if (!spirv)
        return false;

    mShaderModule = device.createShaderModule(spirv.spirv());
    return true;
}

KDGpu::ShaderStage VKShader::getShaderStage() const
{
    return KDGpu::ShaderStage{
        .shaderModule = mShaderModule,
        .stage = getStageFlags(mType),
        .entryPoint = mEntryPoint.toStdString(),
    };
}

int VKShader::getMaxBindingInSet0() const
{
    auto max = -1;
    const auto count = (mInterface ? mInterface->descriptor_binding_count : 0);
    for (auto i = 0u; i < count; ++i)
        if (mInterface->descriptor_bindings[i].set == 0)
            max = std::max(max,
                static_cast<int>(mInterface->descriptor_bindings[i].binding));
    return max;
}

QStringList VKShader::preprocessorDefinitions() const
{
    auto definitions = ShaderBase::preprocessorDefinitions();
    definitions.append("GPUPAD_VULKAN 1");
    return definitions;
}