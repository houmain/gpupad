#include "VKProgram.h"
#include <QRegularExpression>

namespace {
    bool isRaytracingProgram(const Program &program)
    {
        for (const auto &item : program.items)
            if (auto shader = castItem<Shader>(item))
                if (shader->shaderType == Shader::ShaderType::RayGeneration)
                    return true;
        return false;
    }
} // namespace

VKProgram::VKProgram(const Program &program, const Session &session)
    : mItemId(program.id)
    , mSession(session)
{
    mUsedItems += program.id;
    mUsedItems += session.id;
    mCompileShadersSeparately = isRaytracingProgram(program);

    if (mCompileShadersSeparately) {
        for (const auto &item : program.items)
            if (auto shader = castItem<Shader>(item)) {
                mUsedItems += shader->id;
                const auto type = shader->shaderType;
                const auto list = QList<const Shader *>{ shader };
                (type == Shader::ShaderType::Includable ? mIncludableShaders
                                                        : mShaders)
                    .emplace_back(type, list, session);
            }
    } else {
        // create one Shader per stage
        auto shaders = std::map<Shader::ShaderType, QList<const Shader *>>();
        for (const auto &item : program.items)
            if (auto shader = castItem<Shader>(item)) {
                mUsedItems += shader->id;
                shaders[shader->shaderType].append(shader);
            }

        for (const auto &[type, list] : shaders)
            (type == Shader::ShaderType::Includable ? mIncludableShaders
                                                    : mShaders)
                .emplace_back(type, list, session);
    }

    for (auto i = 0; i < static_cast<int>(mShaders.size()); ++i)
        mShaders[i].setShaderIndex(i);
}

bool VKProgram::operator==(const VKProgram &rhs) const
{
    return (std::tie(mShaders, mIncludableShaders)
            == std::tie(rhs.mShaders, rhs.mIncludableShaders)
        && !shaderSessionSettingsDiffer(mSession, rhs.mSession));
}

bool VKProgram::link(VKContext &context)
{
    if (mFailed)
        return false;
    if (!mInterface.empty())
        return true;

    if (mCompileShadersSeparately) {
        for (auto &shader : mShaders) {
            auto inputs = std::vector<Spirv::Input>();
            inputs.push_back(shader.getSpirvCompilerInput(mPrintf));

            auto stages =
                Spirv::compile(mSession, inputs, mItemId, mLinkMessages);
            if (stages.empty()) {
                mFailed = true;
                return false;
            }
            shader.create(context.device, stages[shader.type()]);
            mInterface[shader.getShaderStage().stage] = shader.interface();
        }
    } else {
        auto inputs = std::vector<Spirv::Input>();
        for (auto &shader : mShaders)
            inputs.push_back(shader.getSpirvCompilerInput(mPrintf));

        auto stages = Spirv::compile(mSession, inputs, mItemId, mLinkMessages);
        if (stages.empty()) {
            mFailed = true;
            return false;
        }
        for (auto &shader : mShaders) {
            shader.create(context.device, stages[shader.type()]);
            mInterface[shader.getShaderStage().stage] = shader.interface();
        }
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
