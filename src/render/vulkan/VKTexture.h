#pragma once

#include "VKItem.h"
#include "render/TextureBase.h"

class VKBuffer;
class ScriptEngine;

class VKTexture : public TextureBase
{
public:
    VKTexture(const Texture &texture, ScriptEngine &scriptEngine);
    VKTexture(const Buffer &buffer, VKBuffer *textureBuffer, 
        Texture::Format format, ScriptEngine &scriptEngine);

    KDGpu::Texture &texture() { return mTexture; }
    KDGpu::TextureLayout currentLayout() const { return mCurrentLayout; }
    KDGpu::TextureAspectFlagBits aspectMask() const;

    KDGpu::TextureView &getView(int level = -1, int layer = -1, 
        KDGpu::Format format = KDGpu::Format::UNDEFINED);
    void addUsage(KDGpu::TextureUsageFlags usage);
    bool prepareSampledImage(VKContext &context);
    bool prepareStorageImage(VKContext &context);
    bool prepareAttachment(VKContext &context);
    bool prepareTransferSource(VKContext &context);
    bool clear(VKContext &context, std::array<double, 4> color, double depth, int stencil);
    bool copy(VKContext &context, VKTexture &source);
    bool swap(VKTexture &other);
    bool updateMipmaps(VKContext &context);
    bool deviceCopyModified() const { return mDeviceCopyModified; }
    bool download(VKContext &context);

private:
    struct ViewOptions 
    {
        int level;
        int layer;
        KDGpu::Format format;

        friend bool operator<(const ViewOptions &a, const ViewOptions &b) {
            return std::tie(a.level, a.layer, a.format) <
                   std::tie(b.level, b.layer, b.format);
        }
    };

    void reset(KDGpu::Device& device);
    void createAndUpload(VKContext &context);
    void memoryBarrier(KDGpu::CommandRecorder &commandRecorder, 
        KDGpu::TextureLayout layout, KDGpu::AccessFlags accessMask, 
        KDGpu::PipelineStageFlags stage);

    VKBuffer *mTextureBuffer{ };
    bool mCreated{ };
    KDGpu::TextureUsageFlags mUsage{ };
    ktxVulkanTexture mKtxTexture{ };
    KDGpu::Texture mTexture;
    std::map<ViewOptions, KDGpu::TextureView> mTextureViews;
    KDGpu::TextureLayout mCurrentLayout{ };
    KDGpu::AccessFlags mCurrentAccessMask{ };
    KDGpu::PipelineStageFlags mCurrentStage{ };
};
