#include "VKShader.h"
#include "render/glslang.h"

namespace {
    KDGpu::ShaderStageFlagBits getStageFlags(Shader::ShaderType type)
    {
        using KD = KDGpu::ShaderStageFlagBits;
        using ST = Shader::ShaderType;
        switch (type) {
            case ST::Vertex: return KD::VertexBit;
            case ST::Fragment: return KD::FragmentBit;
            case ST::Geometry: return KD::GeometryBit;
            case ST::TessellationControl: return KD::TessellationControlBit;
            case ST::TessellationEvaluation: return KD::TessellationEvaluationBit;
            case ST::Compute: return KD::ComputeBit;
        }
        return { };
    }
} // namespace

VKShader::VKShader(Shader::ShaderType type, const QList<const Shader*> &shaders,
                   const QString &preamble, const QString &includePaths)
    : ShaderBase(type, shaders, preamble, includePaths)
{
}

bool VKShader::compile(KDGpu::Device &device, VKPrintf *printf)
{
    if (!mPatchedSources.isEmpty())
        return mShaderModule.isValid();

    auto usedFileNames = QStringList();
    mPatchedSources = getPatchedSources(mMessages, usedFileNames, printf);
    if (mPatchedSources.isEmpty())
        return false;

    const auto spirv = glslang::generateSpirVBinary(mLanguage, mType, 
        mPatchedSources, mFileNames, mEntryPoint, mMessages);
    if (spirv.empty())
        return false;

    mShaderModule = device.createShaderModule(spirv);
    mInterface = spirvCross::getInterface(spirv);
    return true;
}

KDGpu::ShaderStage VKShader::getShaderStage() const
{
    return KDGpu::ShaderStage{
        .shaderModule = mShaderModule,
        .stage = getStageFlags(mType),
        //.entryPoint = mEntryPoint.toStdString()
    };
}
