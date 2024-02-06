#pragma once

#include "VKShader.h"
#include "scripting/ScriptEngine.h"
#include "render/spirvCross.h"

class VKProgram
{
public:
    using StageInterface = std::map<KDGpu::ShaderStageFlagBits, spirvCross::Interface>;

    VKProgram(const Program &program, const QString &shaderPreamble, const QString &shaderIncludePaths);
    bool operator==(const VKProgram &rhs) const;

    void link(KDGpu::Device &device);
    std::vector<KDGpu::ShaderStage> getShaderStages();
    ItemId itemId() const { return mItemId; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    const StageInterface& interface() const { return mInterface; }

private:
    ItemId mItemId{ };
    QSet<ItemId> mUsedItems;
    MessagePtrSet mLinkMessages;
    std::vector<VKShader> mShaders;
    MessagePtrSet mPrintfMessages;
    StageInterface mInterface;
    VKPrintf mPrintf;
};
