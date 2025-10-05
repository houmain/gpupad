#pragma once

#if defined(_WIN32)

#include "D3DPrintf.h"
#include "render/ShaderBase.h"

class D3DShader : public ShaderBase
{
public:
    D3DShader(Shader::ShaderType type, const QList<const Shader *> &shaders,
        const Session &session);

    bool compile(ShaderPrintf &printf);
    const ComPtr<ID3DBlob> &binary() const { return mBinary; }
    const ComPtr<ID3D12ShaderReflection> &reflection() const
    {
        return mReflection;
    }

private:
    QStringList preprocessorDefinitions() const override;
    bool compile(const QString &source);
    bool compileD3DCompile(const QString &source);
    bool compileDXC(const QString &source);

    ComPtr<ID3DBlob> mBinary;
    ComPtr<ID3D12ShaderReflection> mReflection;
};

#endif // _WIN32
