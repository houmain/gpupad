#include "VKShader.h"
#include "../glslang.h"

namespace {
    void appendLines(QString &dest, const QString &source) 
    {
        if (source.isEmpty())
            return;
        if (!dest.isEmpty())
            dest += "\n";
        dest += source;
    }

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
{
    Q_ASSERT(!shaders.isEmpty());
    mType = type;
    mItemId = shaders.front()->id;
    mPreamble = preamble;
    mIncludePaths = includePaths;

    for (const Shader *shader : shaders) {
        auto source = QString();
        if (!Singletons::fileCache().getSource(shader->fileName, &source))
            mMessages += MessageList::insert(shader->id,
                MessageType::LoadingFileFailed, shader->fileName);

        mFileNames += shader->fileName;
        mSources += source + "\n";
        mLanguage = shader->language;
        mEntryPoint = shader->entryPoint;
        appendLines(mPreamble, shader->preamble);
        appendLines(mIncludePaths, shader->includePaths);
    }
}

bool VKShader::operator==(const VKShader &rhs) const
{
    return std::tie(mType, mSources, mFileNames, mLanguage, 
                    mEntryPoint, mPreamble, mIncludePaths, mPatchedSources) ==
           std::tie(rhs.mType, rhs.mSources, rhs.mFileNames, rhs.mLanguage, 
                    rhs.mEntryPoint, rhs.mPreamble, rhs.mIncludePaths, rhs.mPatchedSources);
}

bool VKShader::compile(KDGpu::Device &device)
{
    if (mShaderModule.isValid())
        return true;

    const auto spirv = glslang::generateSpirVBinary(mLanguage, mType, 
        mSources, mFileNames, mEntryPoint, mMessages);

    if (!spirv.empty()) {
        mShaderModule = device.createShaderModule(spirv);
        mInterface = spirvCross::getInterface(spirv);
    }
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
