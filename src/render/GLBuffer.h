#ifndef GLBUFFER_H
#define GLBUFFER_H

#include "GLItem.h"
#include "scripting/ScriptEngine.h"

class GLBuffer
{
public:
    GLBuffer(const Buffer &buffer,
        ScriptEngine &scriptEngine, MessagePtrSet &messages);
    void updateUntitledFilename(const GLBuffer &rhs);
    bool operator==(const GLBuffer &rhs) const;

    ItemId itemId() const { return mItemId; }
    const QByteArray &data() const { return mData; }
    const QString &fileName() const { return mFileName; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

    void clear();
    void copy(GLBuffer &source);
    GLuint getReadOnlyBufferId();
    GLuint getReadWriteBufferId();
    void bindReadOnly(GLenum target);
    void unbind(GLenum target);
    bool download();

private:
    void reload();
    void createBuffer();
    void upload();

    ItemId mItemId{ };
    QString mFileName;
    int mSize{ };
    QByteArray mData;
    QSet<ItemId> mUsedItems;
    MessagePtrSet mMessages;
    GLObject mBufferObject;
    bool mSystemCopyModified{ };
    bool mDeviceCopyModified{ };
};

int getBufferSize(const Buffer &buffer,
    ScriptEngine &scriptEngine, MessagePtrSet &messages);

#endif // GLBUFFER_H
