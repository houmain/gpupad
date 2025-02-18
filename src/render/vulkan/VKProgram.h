#pragma once

#include "VKShader.h"
#include "scripting/ScriptEngine.h"

class VKProgram
{
public:
    using StageInterface =
        std::map<KDGpu::ShaderStageFlagBits, Spirv::Interface>;

    VKProgram(const Program &program, const Session &session);
    bool operator==(const VKProgram &rhs) const;

    bool link(KDGpu::Device &device);
    std::vector<KDGpu::ShaderStage> getShaderStages();
    ItemId itemId() const { return mItemId; }
    const Session &session() const { return mSession; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    const StageInterface &interface() const { return mInterface; }
    const std::vector<VKShader> &shaders() const { return mShaders; }
    VKPrintf &printf() { return mPrintf; }

private:
    ItemId mItemId{};
    Session mSession{};
    QSet<ItemId> mUsedItems;
    MessagePtrSet mLinkMessages;
    std::vector<VKShader> mShaders;
    std::vector<VKShader> mIncludableShaders;
    StageInterface mInterface;
    VKPrintf mPrintf;
    bool mCompileShadersSeparately{};
    bool mFailed{};
};
