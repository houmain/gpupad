#include "VKShader.h"

namespace {
    KDGpu::ShaderStageFlagBits getStageFlags(Shader::ShaderType type)
    {
        using ST = Shader::ShaderType;
        using KD = KDGpu::ShaderStageFlagBits;
        switch (type) {
        case ST::Vertex:          return KD::VertexBit;
        case ST::Fragment:        return KD::FragmentBit;
        case ST::Geometry:        return KD::GeometryBit;
        case ST::TessControl:     return KD::TessellationControlBit;
        case ST::TessEvaluation:  return KD::TessellationEvaluationBit;
        case ST::Compute:         return KD::ComputeBit;
        case ST::Task:            return KD::TaskBit;
        case ST::Mesh:            return KD::MeshBit;
        case ST::RayGeneration:   return KD::RaygenBit;
        case ST::RayAnyHit:       return KD::AnyHitBit;
        case ST::RayClosestHit:   return KD::ClosestHitBit;
        case ST::RayMiss:         return KD::MissBit;
        case ST::RayIntersection: return KD::IntersectionBit;
        case ST::RayCallable:     return KD::CallableBit;
        case ST::Includable:      break;
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

void VKShader::create(KDGpu::Device &device, const Spirv &spirv)
{
    Q_ASSERT(spirv);
    mShaderModule = device.createShaderModule(spirv.spirv());
    mInterface = spirv.getInterface();
}

KDGpu::ShaderStage VKShader::getShaderStage() const
{
    return KDGpu::ShaderStage{
        .shaderModule = mShaderModule,
        .stage = getStageFlags(mType),
        .entryPoint = mEntryPoint.toStdString(),
    };
}

QStringList VKShader::preprocessorDefinitions() const
{
    auto definitions = ShaderBase::preprocessorDefinitions();
    definitions.append("GPUPAD_VULKAN 1");
    definitions.append("GPUPAD_GLSLANG 1");
    return definitions;
}