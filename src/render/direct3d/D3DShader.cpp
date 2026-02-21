#include "D3DShader.h"

D3DShader::D3DShader(Shader::ShaderType type,
    const QList<const Shader *> &shaders, const Session &session)
    : ShaderBase(type, shaders, session)
{
}

bool D3DShader::validate()
{
    auto printf = RemoveShaderPrintf{};
    return compile(printf);
}

Reflection D3DShader::getReflection()
{
    validate();
    return reflection();
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
        session.shaderCompiler = Session::ShaderCompiler::DXC;
        session.shaderLanguage = Session::ShaderLanguage::HLSL;
        if (!compile(session, hlsl))
            return false;
        mReflection = Reflection(spirv);
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

    mReflection = generateSpirvReflection(mType, spirvReflection, mD3DReflection.Get());
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

const SpvReflectDescriptorBinding *D3DShader::getSpirvDescriptorBinding(
    const QString &name) const
{
    if (!mReflection)
        return nullptr;

    if (isGlobalUniformBlockName(name))
        for (auto i = 0u; i < mReflection->descriptor_binding_count; ++i) {
            const auto &binding = mReflection->descriptor_bindings[i];
            if (isGlobalUniformBlockName(binding.type_description->type_name))
                return &binding;
        }

    for (auto i = 0u; i < mReflection->descriptor_binding_count; ++i) {
        const auto &binding = mReflection->descriptor_bindings[i];
        if (binding.type_description->type_name == name)
            return &binding;
    }

    for (auto i = 0u; i < mReflection->descriptor_binding_count; ++i) {
        const auto &binding = mReflection->descriptor_bindings[i];
        if (binding.name == name)
            return &binding;
    }

    if (name.startsWith("_")) {
        auto ok = false;
        const auto spirvId = name.mid(1).toUInt(&ok);
        if (ok) {
            for (auto i = 0u; i < mReflection->descriptor_binding_count; ++i) {
                const auto &binding = mReflection->descriptor_bindings[i];
                if (binding.spirv_id == spirvId)
                    return &binding;
            }
        }
    }
    return nullptr;
}
