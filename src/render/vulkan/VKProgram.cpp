#include "VKProgram.h"
#include "VKBuffer.h"
#include "VKTexture.h"
#include "scripting/ScriptEngine.h"
#include <QRegularExpression>

VKProgram::VKProgram(const Program &program, const Session &session)
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
        if (type != Shader::ShaderType::Includable)
            mShaders.emplace_back(type, list, session);
}

bool VKProgram::operator==(const VKProgram &rhs) const
{
    return (std::tie(mShaders) == std::tie(rhs.mShaders)
        && !shaderSessionSettingsDiffer(mSession, rhs.mSession));
}

bool VKProgram::link(KDGpu::Device &device)
{
    if (!mInterface.empty())
        return true;

    auto succeeded = true;
    auto shiftBindingsInSet0 = 0;
    for (auto &shader : mShaders) {
        succeeded &= shader.compile(device, mPrintf, shiftBindingsInSet0);

        if (mSession.autoMapBindings)
            shiftBindingsInSet0 =
                std::max(shiftBindingsInSet0, shader.getMaxBindingInSet0() + 1);
    }
    if (!succeeded)
        return false;

    for (const auto &shader : mShaders)
        mInterface[shader.getShaderStage().stage] = shader.interface();
    return true;
}

std::vector<KDGpu::ShaderStage> VKProgram::getShaderStages()
{
    auto stages = std::vector<KDGpu::ShaderStage>();
    for (const auto &shader : mShaders)
        stages.push_back(shader.getShaderStage());
    return stages;
}
