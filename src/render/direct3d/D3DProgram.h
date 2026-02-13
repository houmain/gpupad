#pragma once

#if defined(_WIN32)

#  include "D3DShader.h"

class D3DProgram
{
public:
    using StageD3DReflection =
        std::map<Shader::ShaderType, ID3D12ShaderReflection *>;

    D3DProgram(const Program &program, const Session &session);
    bool operator==(const D3DProgram &rhs) const;

    bool link(D3DContext &context);
    ItemId itemId() const { return mItemId; }
    const Session &session() const { return mSession; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    const StageD3DReflection &d3dReflection() const { return mD3DReflection; }
    const std::vector<D3DShader> &shaders() const { return mShaders; }
    const D3DShader *getVertexShader() const;
    const SpvReflectDescriptorBinding *getSpirvDescriptorBinding(
        Shader::ShaderType stage, const QString &name) const;
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
    StageD3DReflection mD3DReflection;
    D3DPrintf mPrintf;
    bool mFailed{};
};

#endif // _WIN32
