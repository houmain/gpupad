#pragma once

#include "VKShader.h"
#include "scripting/ScriptEngine.h"
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
    QString offset;
    QString rowCount;
    int stride;
    bool readonly;
};

class VKPipeline
{
public:
    VKPipeline(ItemId itemId, VKProgram *program, 
        VKTarget *target, VKStream *vertexStream);
    ~VKPipeline();

    void clearBindings();
    bool apply(const VKUniformBinding &binding);
    bool apply(const VKSamplerBinding &binding);
    bool apply(const VKImageBinding &binding);
    bool apply(const VKBufferBinding &binding);

    bool createGraphics(VKContext &context, KDGpu::PrimitiveOptions &primitiveOptions);
    bool createCompute(VKContext &context);
    void updateDefaultUniformBlock(VKContext &context, ScriptEngine &scriptEngine);
    bool updateBindings(VKContext &context);
    KDGpu::RenderPassCommandRecorder beginRenderPass(VKContext &context);
    KDGpu::ComputePassCommandRecorder beginComputePass(VKContext &context);
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    struct BindGroup
    {
        std::vector<KDGpu::ResourceBindingLayout> bindings;
        std::vector<KDGpu::BindGroupEntry> resources;
        KDGpu::BindGroup bindGroup;
    };

    struct DefaultUniformBlock
    {
        uint32_t set;
        uint32_t binding;
        uint64_t size;
        KDGpu::Buffer buffer;
    };

    BindGroup &getBindGroup(uint32_t set);
    bool createOrUpdateBindGroup(uint32_t set, uint32_t binding,
        const KDGpu::ResourceBindingLayout &layout);
    void setBindGroupResource(uint32_t set, KDGpu::BindGroupEntry &&resource);
    DefaultUniformBlock &getDefaultUniformBlock(uint32_t set, uint32_t binding);
    bool createLayout(VKContext &context);

    ItemId mItemId;
    VKProgram &mProgram;
    VKTarget *mTarget{ };
    VKStream *mVertexStream{ };

    bool mCreated{ };
    KDGpu::GraphicsPipeline mGraphicsPipeline;
    KDGpu::ComputePipeline mComputePipeline;
    KDGpu::PipelineLayout mPipelineLayout;
    std::vector<BindGroup> mBindGroups;
    std::vector<KDGpu::BindGroupLayout> mBindGroupLayouts;
    std::vector<VKUniformBinding> mUniformBindings;
    std::vector<VKBufferBinding> mBufferBindings;
    std::vector<VKSamplerBinding> mSamplerBindings;
    std::vector<VKImageBinding> mImageBindings;
    std::vector<KDGpu::Sampler> mSamplers;
    std::vector<std::unique_ptr<DefaultUniformBlock>> mDefaultUniformBlocks;
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
};
