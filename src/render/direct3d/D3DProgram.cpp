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
    , mRayTracing(program.rayTracing)
{
    mUsedItems += program.id;
    mUsedItems += session.id;

    auto shaders = std::map<Shader::ShaderType, QList<const Shader *>>();
    for (const auto &item : program.items)
        if (auto shader = castItem<Shader>(item)) {
            mUsedItems += shader->id;
            shaders[shader->shaderType].append(shader);
        }

    for (const auto &[type, list] : shaders) {
        if (!mRayTracing.isEmpty() && isRayTracingShaderType(type)) {
            for (const auto shader : list) {
                if (shader->entryPoint.isEmpty()) {
                    mLinkMessages.insert(shader->id, MessageType::ScriptError,
                        "Explicit DXR configuration requires a named shader "
                        "entry");
                    mFailed = true;
                }
                mShaders.emplace_back(type, QList<const Shader *>{ shader },
                    session);
            }
        } else {
            (type == Shader::ShaderType::Includable ? mIncludableShaders
                                                    : mShaders)
                .emplace_back(type, list, session);
        }
    }
}

bool D3DProgram::operator==(const D3DProgram &rhs) const
{
    return (std::tie(mShaders, mIncludableShaders, mRayTracing)
            == std::tie(rhs.mShaders, rhs.mIncludableShaders, rhs.mRayTracing)
        && !shaderSessionSettingsDiffer(mSession, rhs.mSession));
}

bool D3DProgram::link(D3DContext &context)
{
    if (mFailed)
        return false;

    for (auto &shader : mShaders) {
        mFailed |= !shader.compile(mPrintf);
    }
    return !mFailed;
}

const D3DShader *D3DProgram::getShader(Shader::ShaderType type) const
{
    for (const auto &shader : mShaders)
        if (shader.type() == type)
            return &shader;
    return nullptr;
}

bool D3DProgram::hasShader(Shader::ShaderType type) const
{
    return getShader(type) != nullptr;
}

bool D3DProgram::setupPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC &state)
{
    for (auto &shader : mShaders) {
        if (shader.type() == Shader::ShaderType::Task
            || shader.type() == Shader::ShaderType::Mesh)
            continue;
        if (!setByteCode(state, shader.type(), shader.binary()))
            return false;
    }

    return true;
}

bool D3DProgram::setupPipelineState(D3D12_COMPUTE_PIPELINE_STATE_DESC &state)
{
    for (auto &shader : mShaders)
        if (!setByteCode(state, shader.type(), shader.binary()))
            return false;

    return true;
}
