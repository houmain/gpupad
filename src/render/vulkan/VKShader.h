#pragma once

#include "VKPrintf.h"
#include "render/ShaderBase.h"
#include "render/Spirv.h"

class VKShader : public ShaderBase
{
public:
    VKShader(Shader::ShaderType type, const QList<const Shader*> &shaders,
        const QString &preamble, const QString &includePaths);

    bool compile(KDGpu::Device &device, VKPrintf *printf);
    const Spirv::Interface &interface() const { return mInterface; }
    KDGpu::ShaderStage getShaderStage() const;

private:
    KDGpu::ShaderModule mShaderModule;
    Spirv::Interface mInterface;
};
