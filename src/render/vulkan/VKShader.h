#pragma once

#include "VKPrintf.h"
#include "render/ShaderBase.h"
#include "render/spirvCross.h"

class VKShader : public ShaderBase
{
public:
    VKShader(Shader::ShaderType type, const QList<const Shader*> &shaders,
        const QString &preamble, const QString &includePaths);

    bool compile(KDGpu::Device &device, VKPrintf *printf);
    const spirvCross::Interface &getInterface() const { return mInterface; }
    KDGpu::ShaderStage getShaderStage() const;

private:
    KDGpu::ShaderModule mShaderModule;
    spirvCross::Interface mInterface;
};
