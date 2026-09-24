#pragma once
#if defined(D3D_ENABLED)

#  include "../PipelineBase.h"
#  include "D3DShader.h"
#  include "D3DStream.h"
#  include <map>

class D3DTarget;
class D3DProgram;
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
    bool createMesh(D3DContext &context, Call::PrimitiveType primitiveType,
        D3DTarget *target);
    bool createCompute(D3DContext &context);
    bool createRayTracing(D3DContext &context,
        D3DAccelerationStructure *accelerationStructure);
    bool bindGraphics(D3DContext &context, ScriptEngine &scriptEngine);
    bool bindCompute(D3DContext &context, ScriptEngine &scriptEngine);
    bool bindRayTracing(D3DContext &context, ScriptEngine &scriptEngine);
    void dispatchRays(D3DContext &context, UINT width, UINT height, UINT depth);
    void bindVertexBuffers(D3DContext &context);

private:
    struct DescriptorTableDesc
    {
        D3D12_SHADER_VISIBILITY visibility;
        std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges;
    };

    struct DescriptorHeapEntry
    {
        D3D12_SHADER_VISIBILITY visibility;
        D3D12_DESCRIPTOR_RANGE_TYPE rangeType;
        UINT space;
        UINT bindPoint;
        UINT offset;
        UINT count;
    };

    struct DynamicConstantBuffer
    {
        D3D12_SHADER_VISIBILITY visibility;
        UINT space;
        UINT bindPoint;
        UINT arrayElement;
        std::optional<D3DBuffer> buffer;
    };

    bool setupGraphicsPipelineState(Call::PrimitiveType primitiveType,
        D3DTarget *target, D3D12_GRAPHICS_PIPELINE_STATE_DESC *state);
    static D3D12_SHADER_INPUT_BIND_DESC getD3DBindingDesc(
        const SpvReflectDescriptorBinding &binding);
    static D3D12_DESCRIPTOR_RANGE_TYPE getRangeType(
        D3D_SHADER_INPUT_TYPE inputType);
    bool createInputLayout(std::vector<D3D12_INPUT_ELEMENT_DESC> *inputLayout);
    bool createRootSignature(D3DContext &context);
    bool createRootSignatureRayTracing(D3DContext &context);
    bool createRootSignatureFromTables(D3DContext &context,
        std::vector<DescriptorTableDesc> descriptorTableDescs,
        std::vector<CD3DX12_STATIC_SAMPLER_DESC> staticSamplers);
    void addStaticSampler(const D3D12_SHADER_INPUT_BIND_DESC &bindDesc,
        std::vector<CD3DX12_STATIC_SAMPLER_DESC> *staticSamplers);
    void createDescriptorHeap(D3DContext &context);
    UINT getDescriptorHeapOffset(D3D12_SHADER_VISIBILITY visibility,
        D3D_SHADER_INPUT_TYPE inputType, UINT space, UINT bindPoint) const;
    bool setDescriptors(D3DContext &context, ScriptEngine &scriptEngine);
    DynamicConstantBuffer &getDynamicConstantBuffer(
        D3D12_SHADER_VISIBILITY visibility, UINT space, UINT bindPoint,
        UINT arrayElement);

    D3DProgram &mProgram;
    D3DStream *mVertexStream{ };
    std::vector<const D3DStream::D3DAttribute *> mVertexAttributes;
    D3DAccelerationStructure *mAccelerationStructure{ };
    bool mCreated{ };
    ComPtr<ID3D12PipelineState> mPipelineState;
    ComPtr<ID3D12StateObject> mRayTracingStateObject;
    ComPtr<ID3D12Resource> mShaderBindingTable;
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE mRayGenerationShaderRecord{ };
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE mMissShaderTable{ };
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE mHitGroupTable{ };
    ComPtr<ID3D12GraphicsCommandList> mGraphicsCommandList;
    ComPtr<ID3D12RootSignature> mRootSignature;
    ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;
    std::vector<DescriptorHeapEntry> mDescriptorHeapEntries;
    std::vector<UINT> mDescriptorTableEntries;
    std::vector<std::unique_ptr<DynamicConstantBuffer>> mDynamicConstantBuffers;
};

#endif // D3D_ENABLED
