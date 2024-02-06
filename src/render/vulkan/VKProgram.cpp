#include "VKProgram.h"
#include "VKTexture.h"
#include "VKBuffer.h"
#include "scripting/ScriptEngine.h"
#include <QRegularExpression>

VKProgram::VKProgram(const Program &program, 
      const QString &shaderPreamble, const QString &shaderIncludePaths)
    : mItemId(program.id)
{
    mUsedItems += program.id;

    auto shaders = std::map<Shader::ShaderType, QList<const Shader*>>();
    for (const auto &item : program.items)
        if (auto shader = castItem<Shader>(item)) {
            mUsedItems += shader->id;
            shaders[shader->shaderType].append(shader);
        }

    for (const auto &[type, list] : shaders)
        if (type != Shader::ShaderType::Includable)
            mShaders.emplace_back(type, list, shaderPreamble, shaderIncludePaths);
}

bool VKProgram::operator==(const VKProgram &rhs) const
{
    return (std::tie(mShaders) == 
            std::tie(rhs.mShaders));
}

void VKProgram::link(KDGpu::Device &device) 
{
    mInterface = { };

    auto succeeded = true;
    for (auto &shader : mShaders)
        succeeded &= shader.compile(device, &mPrintf);
    
    if (succeeded) {
        for (const auto &shader : mShaders) {
            const auto &shaderInterface = shader.getInterface();
            auto &stageInterface = mInterface[shader.getShaderStage().stage];
            stageInterface.inputs += shaderInterface.inputs;
            stageInterface.outputs += shaderInterface.outputs;
            stageInterface.textures += shaderInterface.textures;
            stageInterface.images += shaderInterface.images;
            stageInterface.buffers += shaderInterface.buffers;
        }
    }
}

std::vector<KDGpu::ShaderStage> VKProgram::getShaderStages() 
{
    auto stages = std::vector<KDGpu::ShaderStage>();
    for (const auto &shader : mShaders)
        stages.push_back(shader.getShaderStage());
    return stages;
}
