#pragma once

#include "GLTexture.h"

class GLTarget
{
public:
    GLTarget(const Target &target, GLRenderSession &renderSession);
    void setAttachment(int index, GLTexture *texture);

    bool bind();
    void unbind();
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    struct GLAttachment : Attachment
    {
        GLTexture *texture{};
        GLenum attachmentPoint{};
    };

    bool create();
    void applyStates();
    void applyAttachmentStates(const GLAttachment &attachment);

    ItemId mItemId{};
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
    Target::FrontFace mFrontFace{};
    Target::CullMode mCullMode{};
    Target::PolygonMode mPolygonMode{};
    Target::LogicOperation mLogicOperation{};
    QColor mBlendConstant{};
    QMap<int, GLAttachment> mAttachments;
    int mDefaultWidth{};
    int mDefaultHeight{};
    int mDefaultLayers{};
    int mDefaultSamples{};
    GLObject mFramebufferObject;
};
