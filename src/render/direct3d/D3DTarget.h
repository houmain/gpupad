#pragma once

#if defined(_WIN32)

#  include "D3DTexture.h"

class D3DTarget
{
public:
    D3DTarget(const Target &target, D3DRenderSession &renderSession);
    void setTexture(int index, D3DTexture *texture);
    bool hasAttachment(const D3DTexture *texture) const;
    const QSet<ItemId> &usedItems() const { return mUsedItems; };
    bool setupPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC &state);
    bool bind(D3DContext &context);

private:
    struct D3DAttachment : Attachment
    {
        D3DTexture *texture{};
    };

    ItemId mItemId{};
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
    Target::FrontFace mFrontFace{};
    Target::CullMode mCullMode{};
    Target::PolygonMode mPolygonMode{};
    Target::LogicOperation mLogicOperation{};
    QColor mBlendConstant{};
    QMap<int, D3DAttachment> mAttachments;
    int mSamples{};
    int mDefaultWidth{};
    int mDefaultHeight{};
    int mDefaultLayers{};
    bool mFlipViewport{};
};

#endif // _WIN32
