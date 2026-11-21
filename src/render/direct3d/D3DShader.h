#pragma once
#if defined(D3D_ENABLED)

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

private:
    QStringList preprocessorDefinitions() const override;
    bool compile(const Session &session, const QString &source);

    ComPtr<ID3DBlob> mBinary;
    ComPtr<ID3D12ShaderReflection> mD3DReflection;
    ComPtr<ID3D12LibraryReflection> mLibraryReflection;
    Reflection mReflection;
};

Reflection generateSpirvReflection(Shader::ShaderType shaderType,
    const Reflection &spirvReflection, ID3D12ShaderReflection *reflection);
Reflection generateNodeReflection(ID3D12LibraryReflection *reflection);

#endif // D3D_ENABLED
