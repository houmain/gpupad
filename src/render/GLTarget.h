#ifndef GLTARGET_H
#define GLTARGET_H

#include "GLTexture.h"

class GLTarget
{
public:
    explicit GLTarget(const Target &target);
    void setAttachment(int index, GLTexture *texture);

    bool bind();
    void unbind();
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    struct GLAttachment : Attachment {
        Texture::Type type{ };
        GLTexture* texture{ };
        GLenum attachmentPoint{ };
    };

    bool create();
    void applyStates();
    void applyAttachmentStates(const GLAttachment &attachment);

    ItemId mItemId{ };
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
    Target::FrontFace mFrontFace{ };
    Target::CullMode mCullMode{ };
    Target::LogicOperation mLogicOperation{ };
    QColor mBlendConstant{ };
    QMap<int, GLAttachment> mAttachments;
    GLObject mFramebufferObject;
};

#endif // GLTARGET_H
