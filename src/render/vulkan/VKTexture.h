#pragma once

#include "VKItem.h"
#include "render/TextureBase.h"

class VKBuffer;
class ScriptEngine;

class VKTexture : public TextureBase
{
public:
    VKTexture(const Texture &texture, ScriptEngine &scriptEngine);

    KDGpu::Texture &texture() { return mTexture; }
    KDGpu::TextureView &textureView() { return mTextureView; }
    KDGpu::TextureLayout currentLayout() const { return mCurrentLayout; }
    KDGpu::TextureAspectFlagBits aspectMask() const;

    void addUsage(KDGpu::TextureUsageFlags usage);
    bool prepareImageSampler(VKContext &context);
    bool prepareStorageImage(VKContext &context);
    bool prepareAttachment(VKContext &context);
    bool prepareDownload(VKContext &context);
    bool clear(VKContext &context, std::array<double, 4> color, double depth, int stencil);
    bool copy(VKContext &context, VKTexture &source);
    bool swap(VKTexture &other);
    bool deviceCopyModified() const { return mDeviceCopyModified; }
    bool download(VKContext &context);

private:
    void reset(KDGpu::Device& device);
    void createAndUpload(VKContext &context);
    void memoryBarrier(KDGpu::CommandRecorder &commandRecorder, 
        KDGpu::TextureLayout layout, KDGpu::AccessFlags accessMask, 
        KDGpu::PipelineStageFlags stage);

    bool mCreated{ };
    KDGpu::TextureUsageFlags mUsage{ };
    ktxVulkanTexture mKtxTexture{ };
    KDGpu::Texture mTexture;
    KDGpu::TextureView mTextureView;
    KDGpu::TextureLayout mCurrentLayout{ };
    KDGpu::AccessFlags mCurrentAccessMask{ };
    KDGpu::PipelineStageFlags mCurrentStage{ };
};
