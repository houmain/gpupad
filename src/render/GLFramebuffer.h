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

#endif // GLFRAMEBUFFER_H
