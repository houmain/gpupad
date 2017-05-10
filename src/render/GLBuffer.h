#ifndef GLBUFFER_H
#define GLBUFFER_H

#include "PrepareContext.h"
#include "RenderContext.h"

class GLBuffer
{
public:
    explicit GLBuffer(const Buffer &buffer, PrepareContext &context);
    bool operator==(const GLBuffer &rhs) const;

    ItemId itemId() const { return mItemId; }
    GLuint getReadOnlyBufferId(RenderContext &context);
    GLuint getReadWriteBufferId(RenderContext &context);
    void bindReadOnly(RenderContext &context, GLenum target);
    void unbind(RenderContext &context, GLenum target);
    QList<std::pair<QString, QByteArray>> getModifiedData(RenderContext &context);

private:
    void load(MessageList &messages);
    void upload(RenderContext &context);
    bool download(RenderContext &context);

    ItemId mItemId{ };
    QString mFileName;
    QByteArray mData;
    bool mSystemCopyModified{ true };
    bool mDeviceCopyModified{ };
    GLObject mBufferObject;
};

#endif // GLBUFFER_H
