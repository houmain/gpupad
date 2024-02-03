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
    //VKenum access;
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

    bool apply(const VKUniformBinding &binding);
    bool apply(const VKSamplerBinding &binding);
    bool apply(const VKImageBinding &binding);
    bool apply(const VKBufferBinding &binding);
    bool applyPrintfBindings();

    bool createGraphics(VKContext &context, KDGpu::PrimitiveOptions &primitiveOptions);
    bool createCompute(VKContext &context);
    void updateDefaultUniformBlock(VKContext &context, ScriptEngine &scriptEngine);
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

    BindGroup& getBindGroup(uint32_t set);
    void createOrUpdateBindGroup(uint32_t set, uint32_t binding,
        const KDGpu::ResourceBindingLayout &layout);
    bool updateBindings(VKContext &context);
    bool createLayout(VKContext &context);

    VKProgram &mProgram;
    VKTarget *mTarget{ };
    VKStream *mVertexStream{ };

    ItemId mItemId;
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
    std::map<KDGpu::ShaderStageFlagBits, KDGpu::Buffer> mDefaultUniformBlocks;
    bool mAllBuffersBound{ };
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
};
