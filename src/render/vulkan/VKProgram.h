#pragma once

#include "VKShader.h"

class VKProgram
{
public:
    using StageReflection = std::map<KDGpu::ShaderStageFlagBits, Reflection>;

    VKProgram(const Program &program, const Session &session);
    bool operator==(const VKProgram &rhs) const;

    bool link(VKContext &context);
    std::vector<KDGpu::ShaderStage> getShaderStages();
    ItemId itemId() const { return mItemId; }
    const Session &session() const { return mSession; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    const StageReflection &reflection() const { return mReflection; }
    const std::vector<VKShader> &shaders() const { return mShaders; }
    VKPrintf &printf() { return mPrintf; }

private:
    ItemId mItemId{};
    Session mSession{};
    QSet<ItemId> mUsedItems;
    MessagePtrSet mLinkMessages;
    std::vector<VKShader> mShaders;
    std::vector<VKShader> mIncludableShaders;
    StageReflection mReflection;
    VKPrintf mPrintf;
    bool mCompileShadersSeparately{};
    bool mFailed{};
};
