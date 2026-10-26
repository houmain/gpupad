#pragma once

#include "VKTexture.h"

class VKTarget
{
public:
    VKTarget(const Target &target, VKRenderSession &renderSession);
    void setTexture(int index, VKTexture *texture);

    bool validateAttachments(VKContext &context);
    bool prepare(VKContext &context,
        KDGpu::RenderPassCommandRecorderOptions *options);
    std::vector<KDGpu::RenderTargetOptions> getRenderTargetOptions();
    KDGpu::DepthStencilOptions getDepthStencilOptions();
    KDGpu::MultisampleOptions getMultisampleOptions();
    KDGpu::PrimitiveOptions getPrimitiveOptions();

    bool hasAttachment(const VKTexture *texture) const;
    const QSet<ItemId> &usedItems() const { return mUsedItems; };

private:
    struct VKAttachment : Attachment
    {
        VKTexture *texture{};
    };

    KDGpu::Device &mDevice;
    ItemId mItemId{};
    MessageList mMessages;
    QSet<ItemId> mUsedItems;
    Target::FrontFace mFrontFace{};
    Target::CullMode mCullMode{};
    Target::PolygonMode mPolygonMode{};
    Target::LogicOperation mLogicOperation{};
    QColor mBlendConstant{};
    QMap<int, VKAttachment> mAttachments;
    int mSamples{};
    int mDefaultWidth{};
    int mDefaultHeight{};
    int mDefaultLayers{};
    bool mInvalidAttachments{};
};
