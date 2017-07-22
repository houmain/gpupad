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
    struct GLAttachment {
        int level;
        GLTexture* texture;
    };

    bool create();

    ItemId mItemId{ };
    MessagePtr mMessage;
    QSet<ItemId> mUsedItems;
    QMap<int, GLAttachment> mAttachments;
    int mNumColorAttachments{ };
    GLObject mFramebufferObject;
};

#endif // GLTARGET_H
