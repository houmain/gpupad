#pragma once

#if defined(_WIN32)

#  include "D3DPrintf.h"
#  include "render/ShaderBase.h"

class D3DShader : public ShaderBase
{
public:
    D3DShader(Shader::ShaderType type, const QList<const Shader *> &shaders,
        const Session &session);

    bool validate() override;
    Reflection getReflection() override;
    bool compile(PrintfBase &printf);
    const ComPtr<ID3DBlob> &binary() const { return mBinary; }
    ID3D12ShaderReflection *d3dReflection() const
    {
        return mD3DReflection.Get();
    }
    const Reflection &reflection() const { return mReflection; }
    const SpvReflectDescriptorBinding *getSpirvDescriptorBinding(
        const QString &name) const;

private:
    QStringList preprocessorDefinitions() const override;
    bool compile(const Session &session, const QString &source);

    ComPtr<ID3DBlob> mBinary;
    ComPtr<ID3D12ShaderReflection> mD3DReflection;
    Reflection mReflection;
};

Reflection generateSpirvReflection(Shader::ShaderType shaderType,
    ID3D12ShaderReflection *reflection);

#endif // _WIN32
