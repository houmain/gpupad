#ifndef GLBUFFER_H
#define GLBUFFER_H

#include "GLItem.h"

class GLBuffer
{
public:
    explicit GLBuffer(const Buffer &buffer);
    bool operator==(const GLBuffer &rhs) const;

    void clear();
    GLuint getReadOnlyBufferId();
    GLuint getReadWriteBufferId();
    void bindReadOnly(GLenum target);
    void unbind(GLenum target);
    QList<std::pair<ItemId, QByteArray>> getModifiedData();
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    void reload();
    void createBuffer();
    void upload();
    bool download();

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
