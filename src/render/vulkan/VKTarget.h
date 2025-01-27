#pragma once

#include "VKTexture.h"

class VKTarget
{
public:
    VKTarget(const Target &target, const Session &session,
        ScriptEngine &scriptEngine);
    void setTexture(int index, VKTexture *texture);

    KDGpu::RenderPassCommandRecorderOptions prepare(VKContext &context);
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

    ItemId mItemId{};
    MessagePtrSet mMessages;
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
};
