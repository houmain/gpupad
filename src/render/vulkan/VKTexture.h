#pragma once

#include "VKItem.h"

class VKBuffer;
class ScriptEngine;

class VKTexture
{
public:
    VKTexture(const Texture &texture, ScriptEngine &scriptEngine);
    bool operator==(const VKTexture &rhs) const;

    ItemId itemId() const { return mItemId; }
    const QString &fileName() const { return mFileName; }
    TextureKind kind() const { return mKind; }
    Texture::Target target() const { return mTarget; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    int depth() const { return mDepth; }
    int layers() const { return mLayers; }
    Texture::Format format() const { return mFormat; }
    TextureData data() const { return mData; }
    KDGpu::Texture &texture() { return mTexture; }
    KDGpu::TextureView &textureView() { return mTextureView; }
    KDGpu::TextureLayout currentLayout() const { return mCurrentLayout; }
    KDGpu::TextureAspectFlagBits aspectMask() const;
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

    bool prepareImageSampler(VKContext &context);
    bool prepareStorageImage(VKContext &context);
    bool prepareAttachment(VKContext &context);
    bool prepareDownload(VKContext &context);
    bool clear(VKContext &context, std::array<double, 4> color, double depth, int stencil);
    bool copy(VKTexture &source);
    bool swap(VKTexture &other);
    bool updateMipmaps();
    bool deviceCopyModified() const { return mDeviceCopyModified; }
    bool download(VKContext &context);

private:
    void reset(KDGpu::Device& device);
    void createAndUpload(VKContext &context);
    void reload(bool forWriting);
    void memoryBarrier(KDGpu::CommandRecorder &commandRecorder, 
        KDGpu::TextureLayout layout, KDGpu::AccessFlags accessMask, 
        KDGpu::PipelineStageFlags stage);

    ItemId mItemId{ };
    MessagePtrSet mMessages;
    QString mFileName;
    bool mFlipVertically{ };
    Texture::Target mTarget{ };
    Texture::Format mFormat{ };
    int mWidth{ };
    int mHeight{ };
    int mDepth{ };
    int mLayers{ };
    int mSamples{ };
    TextureData mData;
    bool mDataWritten{ };
    QSet<ItemId> mUsedItems;
    TextureKind mKind{ };
    bool mCreated{ };
    ktxVulkanTexture mKtxTexture{ };
    KDGpu::Texture mTexture;
    KDGpu::TextureView mTextureView;
    KDGpu::TextureLayout mCurrentLayout{ };
    KDGpu::AccessFlags mCurrentAccessMask{ };
    KDGpu::PipelineStageFlags mCurrentStage{ };
    bool mSystemCopyModified{ };
    bool mDeviceCopyModified{ };
    bool mMipmapsInvalidated{ };
};
