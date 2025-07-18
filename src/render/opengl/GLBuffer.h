#pragma once

#include "GLItem.h"
#include "render/BufferBase.h"

class GLBuffer : public BufferBase
{
public:
    explicit GLBuffer(int size);
    GLBuffer(const Buffer &buffer, GLRenderSession &renderSession);

    QByteArray &getWriteableData();
    void clear();
    void copy(GLBuffer &source);
    bool swap(GLBuffer &other);
    GLuint getReadOnlyBufferId();
    GLuint getReadWriteBufferId();
    void bindReadOnly(GLenum target);
    void bindIndexedRange(GLenum target, int index, int offset, int size,
        bool readonly);
    void unbind(GLenum target);
    bool download(GLContext &context, bool checkModification);

private:
    void reload();
    void createBuffer();
    void upload();

    GLObject mBufferObject;
};
