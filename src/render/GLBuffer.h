#ifndef GLBUFFER_H
#define GLBUFFER_H

#include "GLItem.h"

class GLBuffer
{
public:
    explicit GLBuffer(const Buffer &buffer);
    bool operator==(const GLBuffer &rhs) const;

    ItemId itemId() const { return mItemId; }
    QByteArray data() const { return mData; }
    const QString &fileName() const { return mFileName; }
    void clear();
    void copy(GLBuffer &source);
    GLuint getReadOnlyBufferId();
    GLuint getReadWriteBufferId();
    void bindReadOnly(GLenum target);
    void unbind(GLenum target);
    bool download();
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    void reload();
    void createBuffer();
    void upload();

    ItemId mItemId{ };
    QString mFileName;
    int mOffset{ };
    int mSize{ };
    QByteArray mData;
    QSet<ItemId> mUsedItems;
    MessagePtrSet mMessages;
    GLObject mBufferObject;
    bool mSystemCopyModified{ };
    bool mDeviceCopyModified{ };
};

#endif // GLBUFFER_H
