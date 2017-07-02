#ifndef GLBUFFER_H
#define GLBUFFER_H

#include "GLItem.h"

class GLBuffer
{
public:
    explicit GLBuffer(const Buffer &buffer);
    bool operator==(const GLBuffer &rhs) const;

    GLuint getReadOnlyBufferId();
    GLuint getReadWriteBufferId();
    void bindReadOnly(GLenum target);
    void unbind(GLenum target);
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    QList<std::pair<QString, QByteArray>> getModifiedData();

private:
    void load();
    void upload();
    bool download();

    ItemId mItemId{ };
    QString mFileName;
    QByteArray mData;

    QSet<ItemId> mUsedItems;
    MessagePtr mMessage;
    GLObject mBufferObject;
    bool mSystemCopyModified{ };
    bool mDeviceCopyModified{ };
};

#endif // GLBUFFER_H
