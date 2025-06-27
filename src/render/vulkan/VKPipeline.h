#pragma once

#include "VKShader.h"
#include "scripting/ScriptEngine.h"
#include <span>
#include <map>

class VKTarget;
class VKProgram;
class VKStream;
class VKAccelerationStructure;

class VKPipeline
{
public:
    VKPipeline(ItemId itemId, VKProgram *program);
    ~VKPipeline();

    void setBindings(Bindings &&bindings);
    bool createGraphics(VKContext &context,
        KDGpu::PrimitiveOptions &primitiveOptions, VKTarget *target,
        VKStream *vertexStream);
    bool createCompute(VKContext &context);
    bool createRayTracing(VKContext &context,
        VKAccelerationStructure *accelStruct);
    bool updateBindings(VKContext &context, ScriptEngine &scriptEngine);
    KDGpu::RenderPassCommandRecorder beginRenderPass(VKContext &context,
        bool flipViewport);
    KDGpu::ComputePassCommandRecorder beginComputePass(VKContext &context);
    KDGpu::RayTracingPassCommandRecorder beginRayTracingPass(
        VKContext &context);
    bool updatePushConstants(KDGpu::RenderPassCommandRecorder &renderPass,
        ScriptEngine &scriptEngine);
    bool updatePushConstants(KDGpu::ComputePassCommandRecorder &computePass,
        ScriptEngine &scriptEngine);
    bool updatePushConstants(
        KDGpu::RayTracingPassCommandRecorder &rayTracingPass,
        ScriptEngine &scriptEngine);

    const KDGpu::RayTracingShaderBindingTable &rayTracingShaderBindingTable()
        const
    {
        return mRayTracingShaderBindingTable;
    }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    struct BindGroup
    {
        std::vector<KDGpu::ResourceBindingLayout> bindings;
        std::vector<KDGpu::BindGroupEntry> resources;
        KDGpu::BindGroup bindGroup;
        uint32_t maxVariableArrayLength{ 0 };
    };

    struct DynamicUniformBuffer
    {
        uint32_t set;
        uint32_t binding;
        uint32_t arrayElement;
        uint64_t size;
        KDGpu::Buffer buffer;
    };

    const KDGpu::Sampler &getSampler(VKContext &context,
        const SamplerBinding &samplerBinding);
    BindGroup &getBindGroup(uint32_t set);
    bool createOrUpdateBindGroup(uint32_t set, uint32_t binding,
        const KDGpu::ResourceBindingLayout &layout);
    void setBindGroupResource(uint32_t set, bool isVariableLengthArray,
        KDGpu::BindGroupEntry &&resource);
    DynamicUniformBuffer &getDynamicUniformBuffer(uint32_t set,
        uint32_t binding, uint32_t arrayElement);
    MessageType updateBindings(VKContext &context,
        const SpvReflectDescriptorBinding &desc, uint32_t arrayElement,
        bool isVariableLengthArray, ScriptEngine &scriptEngine);
    bool updateDynamicBufferBindings(VKContext &context,
        const SpvReflectDescriptorBinding &desc, uint32_t arrayElement,
        ScriptEngine &scriptEngine);
    bool createLayout(VKContext &context);
    void applyBufferMemberBinding(std::span<std::byte> bufferData,
        const SpvReflectBlockVariable &member, const UniformBinding &binding,
        int memberOffset, int elementOffset, int count,
        ScriptEngine &scriptEngine);
    bool applyBufferMemberBindings(std::span<std::byte> bufferData,
        const QString &name, const SpvReflectBlockVariable &member,
        int memberOffset, ScriptEngine &scriptEngine);
    bool applyBufferMemberBindings(std::span<std::byte> bufferData,
        const SpvReflectBlockVariable &block, uint32_t arrayElement,
        ScriptEngine &scriptEngine);
    bool hasPushConstants() const;
    bool updatePushConstants(ScriptEngine &scriptEngine);

    ItemId mItemId;
    VKProgram &mProgram;
    VKTarget *mTarget{};
    VKStream *mVertexStream{};
    VKAccelerationStructure *mAccelerationStructure{};

    bool mCreated{};
    KDGpu::GraphicsPipeline mGraphicsPipeline;
    KDGpu::ComputePipeline mComputePipeline;
    KDGpu::RayTracingPipeline mRayTracingPipeline;
    KDGpu::PipelineLayout mPipelineLayout;
    std::vector<BindGroup> mBindGroups;
    std::vector<KDGpu::BindGroupLayout> mBindGroupLayouts;
    Bindings mBindings;
    std::map<KDGpu::SamplerOptions, KDGpu::Sampler> mSamplers;
    std::vector<std::unique_ptr<DynamicUniformBuffer>> mDynamicUniformBuffers;
    std::vector<std::byte> mPushConstantData;
    KDGpu::PushConstantRange mPushConstantRange{};
    KDGpu::RayTracingShaderBindingTable mRayTracingShaderBindingTable;
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
};
