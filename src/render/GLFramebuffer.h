#ifndef GLFRAMEBUFFER_H
#define GLFRAMEBUFFER_H

#include "GLTexture.h"

class GLFramebuffer
{
public:
    explicit GLFramebuffer(const Framebuffer &framebuffer);
    void setAttachment(int index, GLTexture *texture);

    bool bind();
    void unbind();
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    bool create();

    ItemId mItemId{ };
    MessagePtr mMessage;
    QSet<ItemId> mUsedItems;
    QList<GLTexture*> mTextures;
    int mWidth{ };
    int mHeight{ };
    int mNumColorAttachments{ };
    GLObject mFramebufferObject;
};

#endif // GLFRAMEBUFFER_H
