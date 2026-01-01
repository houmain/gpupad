#pragma once

#if defined(_WIN32)

#include "D3DShader.h"
#include "scripting/ScriptEngine.h"
#include <span>
#include <map>

class D3DTarget;
class D3DProgram;
class D3DStream;
class D3DBuffer;
class D3DTexture;
class D3DAccelerationStructure;

class D3DPipeline
{
public:
    D3DPipeline(ItemId itemId, D3DProgram *program);
    ~D3DPipeline();

    void setBindings(Bindings &&bindings);
    bool createGraphics(D3DContext &context, Call::PrimitiveType primitiveType,
        D3DTarget *target, D3DStream *vertexStream);
    bool createCompute(D3DContext &context);
    bool bindGraphics(D3DContext &context, ScriptEngine &scriptEngine);
    bool bindCompute(D3DContext &context, ScriptEngine &scriptEngine);
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    bool createInputLayout(std::vector<D3D12_INPUT_ELEMENT_DESC> *inputLayout);
    void createRootSignature(D3DContext &context);
    void createDescriptorHeap(D3DContext &context);
    void createGlobalConstantBuffers(D3DContext &context);
    void updateGlobalConstantBuffers(D3DContext &context,
        ScriptEngine &scriptEngine);
    D3DBuffer *getGlobalConstantBuffer(Shader::ShaderType stage);
    bool setDescriptors(D3DContext &context);

    ItemId mItemId;
    D3DProgram &mProgram;
    D3DStream *mVertexStream{};
    D3DAccelerationStructure *mAccelerationStructure{};

    bool mCreated{};
    ComPtr<ID3D12PipelineState> mPipelineState;
    ComPtr<ID3D12GraphicsCommandList> mGraphicsCommandList;
    ComPtr<ID3D12RootSignature> mRootSignature;
    UINT mDescriptorCount{};
    ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;
    std::vector<int> mDescriptorTableEntries;
    Bindings mBindings;
    std::map<Shader::ShaderType, std::unique_ptr<D3DBuffer>>
        mGlobalConstantBuffers;
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
};

#endif // _WIN32
