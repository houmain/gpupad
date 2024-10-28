#pragma once

#include "VKPrintf.h"
#include "render/ShaderBase.h"
#include "render/Spirv.h"

class VKShader : public ShaderBase
{
public:
    VKShader(Shader::ShaderType type,
        const QList<const Shader *> &shaders, const Session &session);

    bool compile(KDGpu::Device &device, VKPrintf *printf,
        int shiftBindingsInSet0);
    const Spirv::Interface &interface() const { return mInterface; }
    KDGpu::ShaderStage getShaderStage() const;
    int getMaxBindingInSet0() const;

private:
    QStringList preprocessorDefinitions() const override;

    KDGpu::ShaderModule mShaderModule;
    Spirv::Interface mInterface;
};
