#pragma once

#include "VKShader.h"
#include "scripting/ScriptEngine.h"
#include <span>
#include <map>

class VKTexture;
class VKBuffer;
class VKTarget;
class VKProgram;
class VKStream;

struct VKUniformBinding
{
    ItemId bindingItemId;
    QString name;
    Binding::BindingType type;
    QStringList values;
    bool transpose;
};

struct VKSamplerBinding
{
    ItemId bindingItemId;
    QString name;
    VKTexture *texture;
    Binding::Filter minFilter;
    Binding::Filter magFilter;
    bool anisotropic;
    Binding::WrapMode wrapModeX;
    Binding::WrapMode wrapModeY;
    Binding::WrapMode wrapModeZ;
    QColor borderColor;
    Binding::ComparisonFunc comparisonFunc;
};

struct VKImageBinding
{
    ItemId bindingItemId;
    QString name;
    VKTexture *texture;
    int level;
    int layer;
    Binding::ImageFormat format;
};

struct VKBufferBinding
{
    ItemId bindingItemId;
    QString name;
    VKBuffer *buffer;
    ItemId blockItemId;
    QString offset;
    QString rowCount;
    int stride;
};

struct VKBindings
{
    std::map<QString, VKUniformBinding> uniforms;
    std::map<QString, VKSamplerBinding> samplers;
    std::map<QString, VKImageBinding> images;
    std::map<QString, VKBufferBinding> buffers;
};

class VKPipeline
{
public:
    VKPipeline(ItemId itemId, VKProgram *program, VKTarget *target,
        VKStream *vertexStream);
    ~VKPipeline();

    void setBindings(VKBindings &&bindings);
    bool createGraphics(VKContext &context,
        KDGpu::PrimitiveOptions &primitiveOptions);
    bool createCompute(VKContext &context);
    bool createRayTracing(VKContext &context);
    void createRayTracingAccelerationStructure(VKContext &context,
        VKBuffer &aabbBuffer);
    bool updateBindings(VKContext &context, ScriptEngine &scriptEngine);
    KDGpu::RenderPassCommandRecorder beginRenderPass(VKContext &context);
    KDGpu::ComputePassCommandRecorder beginComputePass(VKContext &context);
    KDGpu::RayTracingPassCommandRecorder beginRayTracingPass(
        VKContext &context);
    void updatePushConstants(KDGpu::RenderPassCommandRecorder &renderPass,
        ScriptEngine &scriptEngine);
    void updatePushConstants(KDGpu::ComputePassCommandRecorder &computePass,
        ScriptEngine &scriptEngine);
    void updatePushConstants(
        KDGpu::RayTracingPassCommandRecorder &rayTracingPass,
        ScriptEngine &scriptEngine);

    const KDGpu::RayTracingShaderBindingTable &rayTracingShaderBindingTable() const
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
        const VKSamplerBinding &samplerBinding);
    BindGroup &getBindGroup(uint32_t set);
    bool createOrUpdateBindGroup(uint32_t set, uint32_t binding,
        const KDGpu::ResourceBindingLayout &layout);
    void setBindGroupResource(uint32_t set, KDGpu::BindGroupEntry &&resource);
    DynamicUniformBuffer &getDynamicUniformBuffer(uint32_t set,
        uint32_t binding, uint32_t arrayElement);
    MessageType updateBindings(VKContext &context,
        const SpvReflectDescriptorBinding &desc, uint32_t arrayElement,
        ScriptEngine &scriptEngine);
    bool updateDynamicBufferBindings(VKContext &context,
        const SpvReflectDescriptorBinding &desc, uint32_t arrayElement,
        ScriptEngine &scriptEngine);
    bool createLayout(VKContext &context);
    void applyBufferMemberBinding(std::span<std::byte> bufferData,
        const SpvReflectBlockVariable &member, const VKUniformBinding &binding,
        int memberOffset, int elementOffset, int count,
        ScriptEngine &scriptEngine);
    bool applyBufferMemberBindings(std::span<std::byte> bufferData,
        const QString &name, const SpvReflectBlockVariable &member,
        int memberOffset, ScriptEngine &scriptEngine);
    bool applyBufferMemberBindings(std::span<std::byte> bufferData,
        const SpvReflectBlockVariable &block, uint32_t arrayElement,
        ScriptEngine &scriptEngine);
    bool updatePushConstants(ScriptEngine &scriptEngine);

    ItemId mItemId;
    VKProgram &mProgram;
    VKTarget *mTarget{};
    VKStream *mVertexStream{};

    bool mCreated{};
    KDGpu::GraphicsPipeline mGraphicsPipeline;
    KDGpu::ComputePipeline mComputePipeline;
    KDGpu::RayTracingPipeline mRayTracingPipeline;
    KDGpu::PipelineLayout mPipelineLayout;
    std::vector<BindGroup> mBindGroups;
    std::vector<KDGpu::BindGroupLayout> mBindGroupLayouts;
    VKBindings mBindings;
    std::map<KDGpu::SamplerOptions, KDGpu::Sampler> mSamplers;
    std::vector<std::unique_ptr<DynamicUniformBuffer>> mDynamicUniformBuffers;
    std::vector<std::byte> mPushConstantData;
    KDGpu::PushConstantRange mPushConstantRange{};
    KDGpu::RayTracingShaderBindingTable mRayTracingShaderBindingTable;
    KDGpu::AccelerationStructure mBottomLevelAs;
    KDGpu::AccelerationStructure mTopLevelAs;
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
};
