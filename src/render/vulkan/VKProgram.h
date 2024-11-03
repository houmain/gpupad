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
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    const StageInterface &interface() const { return mInterface; }
    VKPrintf &printf() { return mPrintf; }

private:
    ItemId mItemId{};
    QSet<ItemId> mUsedItems;
    MessagePtrSet mLinkMessages;
    std::vector<VKShader> mShaders;
    StageInterface mInterface;
    VKPrintf mPrintf;
};
