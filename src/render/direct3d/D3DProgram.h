#pragma once
#if defined(D3D_ENABLED)

#  include "D3DShader.h"

class D3DProgram
{
public:
    D3DProgram(const Program &program, const Session &session);
    bool operator==(const D3DProgram &rhs) const;

    bool link(D3DContext &context);
    ItemId itemId() const { return mItemId; }
    const Session &session() const { return mSession; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    const std::vector<D3DShader> &shaders() const { return mShaders; }
    const D3DShader *getShader(Shader::ShaderType type) const;
    bool hasShader(Shader::ShaderType type) const;
    D3DPrintf &printf() { return mPrintf; }
    bool setupPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC &state);
    bool setupPipelineState(D3D12_COMPUTE_PIPELINE_STATE_DESC &state);

private:
    ItemId mItemId{};
    Session mSession{};
    QSet<ItemId> mUsedItems;
    MessagePtrSet mLinkMessages;
    std::vector<D3DShader> mShaders;
    std::vector<D3DShader> mIncludableShaders;
    D3DPrintf mPrintf;
    bool mFailed{};
};

#endif // D3D_ENABLED
