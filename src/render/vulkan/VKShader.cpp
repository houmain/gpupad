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
    const QList<const Shader *> &shaders, const QString &preamble,
    const QString &includePaths)
    : ShaderBase(type, shaders, preamble, includePaths)
{
}

bool VKShader::compile(KDGpu::Device &device, VKPrintf *printf,
    int shiftBindingsInSet0)
{
    if (!mPatchedSources.isEmpty())
        return mShaderModule.isValid();

    auto usedFileNames = QStringList();
    mPatchedSources = getPatchedSources(mMessages, usedFileNames, printf);
    if (mPatchedSources.isEmpty())
        return false;

    auto spirv = Spirv::generate(mLanguage, mType, mPatchedSources, mFileNames,
        mEntryPoint, shiftBindingsInSet0, mMessages);
    if (!spirv)
        return false;

    mShaderModule = device.createShaderModule(spirv.spirv());
    mInterface = spirv.getInterface();
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
