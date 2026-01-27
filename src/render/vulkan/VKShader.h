#pragma once

#include "VKPrintf.h"
#include "render/ShaderBase.h"

class VKShader : public ShaderBase
{
public:
    VKShader(Shader::ShaderType type, const QList<const Shader *> &shaders,
        const Session &session);

    void setShaderIndex(int index) { mShaderIndex = index; }
    int shaderIndex() const { return mShaderIndex; }
    void create(KDGpu::Device &device, const Spirv &spirv);
    const Reflection &reflection() const { return mReflection; }
    KDGpu::ShaderStage getShaderStage() const;

private:
    QStringList preprocessorDefinitions() const override;

    KDGpu::ShaderModule mShaderModule;
    Reflection mReflection;
    int mShaderIndex{ -1 };
};
