#include "D3DShader.h"

D3DShader::D3DShader(Shader::ShaderType type,
    const QList<const Shader *> &shaders, const Session &session)
    : ShaderBase(type, shaders, session)
{
}

bool D3DShader::validate()
{
    auto printf = RemoveShaderPrintf{ };
    return compile(printf);
}

Reflection D3DShader::getReflection()
{
    validate();
    return mReflection;
}

bool D3DShader::compile(PrintfBase &printf)
{
    if (mBinary)
        return true;

    if (mSession.shaderCompiler == Session::ShaderCompiler::glslang) {
        const auto spirv = compileSpirv(printf);
        if (spirv.empty())
            return false;
        const auto hlsl =
            ShaderCompiler::generateHLSL(spirv, mItemId, mMessages);
        if (hlsl.isEmpty())
            return false;
        auto session = mSession;
#if defined(DXC_ENABLED)
        session.shaderCompiler = Session::ShaderCompiler::DXC;
#else
        session.shaderCompiler = Session::ShaderCompiler::D3DCompiler;
#endif
        session.shaderLanguage = Session::ShaderLanguage::HLSL;
        if (!compile(session, hlsl))
            return false;
        const auto spirvReflection = Reflection(spirv);
        mReflection = (mD3DReflection
                ? generateSpirvReflection(mType, spirvReflection,
                      mD3DReflection.Get())
                : Reflection(spirvReflection));
        return true;
    }

    const auto patchedSources = getPatchedSourcesHLSL(printf);
    if (patchedSources.isEmpty())
        return false;

    if (!compile(mSession, patchedSources.join("\n")))
        return false;

    // also generate SPIR-V for completing type information missing in D3D reflection
    const auto spirv = compileSpirv(printf);
    const auto spirvReflection = Reflection(spirv);

    mReflection = (mD3DReflection ? generateSpirvReflection(mType,
                                        spirvReflection, mD3DReflection.Get())
                                  : Reflection(spirvReflection));
    return true;
}

bool D3DShader::compile(const Session &session, const QString &source)
{
    if (source.isEmpty())
        return false;

    const auto input = ShaderCompiler::Input{
        .shaderType = mType,
        .sources = { source },
        .fileNames = { mFileNames[0] },
        .entryPoint = mEntryPoint,
        .includePaths = mIncludePaths,
        .itemId = mItemId,
    };
    return ShaderCompiler::compileDXIL(session, input, mMessages, mBinary,
        mD3DReflection);
}

QStringList D3DShader::preprocessorDefinitions() const
{
    auto definitions = ShaderBase::preprocessorDefinitions();
    definitions.append("GPUPAD_DIRECT3D 1");
    return definitions;
}
