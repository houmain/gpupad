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
        (type == Shader::ShaderType::Includable ? mIncludableShaders : mShaders)
            .emplace_back(type, list, session);
}

bool VKProgram::operator==(const VKProgram &rhs) const
{
    return (std::tie(mShaders, mIncludableShaders)
            == std::tie(rhs.mShaders, rhs.mIncludableShaders)
        && !shaderSessionSettingsDiffer(mSession, rhs.mSession));
}

bool VKProgram::link(KDGpu::Device &device)
{
    if (mFailed)
        return false;
    if (!mInterface.empty())
        return true;

    auto inputs = std::vector<Spirv::Input>();
    for (auto &shader : mShaders)
        inputs.push_back(shader.getSpirvCompilerInput(mPrintf));

    auto stages = Spirv::compile(mSession, inputs, mItemId, mLinkMessages);
    if (stages.empty()) {
        mFailed = true;
        return false;
    }
    for (auto &shader : mShaders) {
        shader.create(device, stages[shader.type()]);
        mInterface[shader.getShaderStage().stage] = shader.interface();
    }
    return true;
}

std::vector<KDGpu::ShaderStage> VKProgram::getShaderStages()
{
    auto stages = std::vector<KDGpu::ShaderStage>();
    for (const auto &shader : mShaders)
        stages.push_back(shader.getShaderStage());
    return stages;
}
