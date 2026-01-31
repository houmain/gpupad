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

    bool createInputLayout(std::vector<D3D12_INPUT_ELEMENT_DESC> *inputLayout);
    bool createRootSignature(D3DContext &context);
    void createDescriptorHeap(D3DContext &context);
    UINT getDescriptorHeapOffset(D3D12_SHADER_VISIBILITY visibility,
        D3D_SHADER_INPUT_TYPE inputType, UINT space, UINT bindPoint) const;
    bool setDescriptors(D3DContext &context, ScriptEngine &scriptEngine);
    DynamicConstantBuffer &getDynamicConstantBuffer(
        D3D12_SHADER_VISIBILITY visibility, UINT space, UINT bindPoint,
        UINT arrayElement);

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
    std::vector<std::unique_ptr<DynamicConstantBuffer>> mDynamicConstantBuffers;
};

#endif // _WIN32
