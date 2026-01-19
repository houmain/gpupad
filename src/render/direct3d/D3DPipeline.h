#pragma once

#if defined(_WIN32)

#  include "../PipelineBase.h"
#  include "D3DShader.h"
#  include <map>

class D3DTarget;
class D3DProgram;
class D3DStream;
class D3DBuffer;
class D3DTexture;
class D3DAccelerationStructure;

class D3DPipeline : public PipelineBase
{
public:
    D3DPipeline(ItemId itemId, D3DProgram *program);
    ~D3DPipeline();

    bool createGraphics(D3DContext &context, Call::PrimitiveType primitiveType,
        D3DTarget *target, D3DStream *vertexStream);
    bool createCompute(D3DContext &context);
    bool bindGraphics(D3DContext &context, ScriptEngine &scriptEngine);
    bool bindCompute(D3DContext &context, ScriptEngine &scriptEngine);

private:
    struct DescriptorHeapEntry
    {
        D3D12_SHADER_VISIBILITY visibility;
        UINT space;
        UINT bindPoint;
        UINT offset;
    };

    struct DynamicConstantBuffer
    {
        uint32_t space;
        uint32_t binding;
        uint32_t arrayElement;
        std::optional<D3DBuffer> buffer;
    };

    bool createInputLayout(std::vector<D3D12_INPUT_ELEMENT_DESC> *inputLayout);
    bool createRootSignature(D3DContext &context);
    void createDescriptorHeap(D3DContext &context);
    void createGlobalConstantBuffers(D3DContext &context);
    void updateGlobalConstantBuffers(D3DContext &context,
        ScriptEngine &scriptEngine);
    D3DBuffer *getGlobalConstantBuffer(Shader::ShaderType stage);
    UINT getDescriptorHeapOffset(D3D12_SHADER_VISIBILITY visibility, UINT space,
        UINT bindPoint) const;
    bool setDescriptors(D3DContext &context, ScriptEngine &scriptEngine);
    DynamicConstantBuffer &getDynamicConstantBuffer(uint32_t space,
        uint32_t binding, uint32_t arrayElement);
    ComPtr<ID3D12Resource> updateDynamicBufferBindings(D3DContext &context,
        const SpvReflectDescriptorBinding &desc, uint32_t arrayElement,
        ScriptEngine &scriptEngine);

    D3DProgram &mProgram;
    D3DStream *mVertexStream{};
    D3DAccelerationStructure *mAccelerationStructure{};
    bool mCreated{};
    ComPtr<ID3D12PipelineState> mPipelineState;
    ComPtr<ID3D12GraphicsCommandList> mGraphicsCommandList;
    ComPtr<ID3D12RootSignature> mRootSignature;
    ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;
    std::vector<DescriptorHeapEntry> mDescriptorHeapEntries;
    std::vector<UINT> mDescriptorTableEntries;
    std::map<Shader::ShaderType, std::unique_ptr<D3DBuffer>>
        mGlobalConstantBuffers;
    std::vector<std::unique_ptr<DynamicConstantBuffer>> mDynamicConstantBuffers;
};

#endif // _WIN32
