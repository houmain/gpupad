#include "D3DProgram.h"
#include <QRegularExpression>

namespace {
    bool setByteCode(D3D12_GRAPHICS_PIPELINE_STATE_DESC &state,
        Shader::ShaderType type, const ComPtr<ID3DBlob> &binary)
    {
        if (!binary)
            return false;

        const auto bytecode = D3D12_SHADER_BYTECODE{
            reinterpret_cast<BYTE *>(binary->GetBufferPointer()),
            binary->GetBufferSize(),
        };
        switch (type) {
        case Shader::ShaderType::Vertex:         state.VS = bytecode; break;
        case Shader::ShaderType::Fragment:       state.PS = bytecode; break;
        case Shader::ShaderType::Geometry:       state.GS = bytecode; break;
        case Shader::ShaderType::TessControl:    state.HS = bytecode; break;
        case Shader::ShaderType::TessEvaluation: state.DS = bytecode; break;
        default:                                 return false;
        }
        return true;
    }

    bool setByteCode(D3D12_COMPUTE_PIPELINE_STATE_DESC &state,
        Shader::ShaderType type, const ComPtr<ID3DBlob> &binary)
    {
        if (!binary)
            return false;

        const auto bytecode = D3D12_SHADER_BYTECODE{
            reinterpret_cast<BYTE *>(binary->GetBufferPointer()),
            binary->GetBufferSize(),
        };
        switch (type) {
        case Shader::ShaderType::Compute: state.CS = bytecode; break;
        default:                          return false;
        }
        return true;
    }
} // namespace

D3DProgram::D3DProgram(const Program &program, const Session &session)
    : mItemId(program.id)
    , mSession(session)
{
    mUsedItems += program.id;
    mUsedItems += session.id;

    auto shaders = std::map<Shader::ShaderType, QList<const Shader *>>();
    for (const auto &item : program.items)
        if (auto shader = castItem<Shader>(item)) {
            mUsedItems += shader->id;
            shaders[shader->shaderType].append(shader);
        }

    for (const auto &[type, list] : shaders)
        (type == Shader::ShaderType::Includable ? mIncludableShaders : mShaders)
            .emplace_back(type, list, session);
}

bool D3DProgram::operator==(const D3DProgram &rhs) const
{
    return (std::tie(mShaders, mIncludableShaders)
            == std::tie(rhs.mShaders, rhs.mIncludableShaders)
        && !shaderSessionSettingsDiffer(mSession, rhs.mSession));
}

bool D3DProgram::link(D3DContext &context)
{
    if (mFailed)
        return false;

    for (auto &shader : mShaders) {
        mFailed |= !shader.compile(mPrintf);
        mReflection[shader.type()] = shader.reflection().Get();
    }
    return !mFailed;
}

const D3DShader *D3DProgram::getVertexShader() const
{
    for (const auto &shader : mShaders)
        if (shader.type() == Shader::ShaderType::Vertex)
            return &shader;
    return nullptr;
}

QString D3DProgram::getBufferBindingName(Shader::ShaderType stage,
    const QString &name) const
{
    for (const auto &shader : mShaders)
        if (shader.type() == stage)
            return shader.getBufferBindingName(name);
    return name;
}

bool D3DProgram::setupPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC &state)
{
    for (auto &shader : mShaders)
        if (!setByteCode(state, shader.type(), shader.binary()))
            return false;

    return true;
}

bool D3DProgram::setupPipelineState(D3D12_COMPUTE_PIPELINE_STATE_DESC &state)
{
    for (auto &shader : mShaders)
        if (!setByteCode(state, shader.type(), shader.binary()))
            return false;

    return true;
}
