#pragma once
#if defined(OPENGL_ENABLED)

#  include "GLContext.h"
#  include "render/BufferBase.h"

class GLBuffer : public BufferBase
{
public:
    explicit GLBuffer(int size);
    GLBuffer(const Buffer &buffer, GLRenderSession &renderSession);

    QByteArray &getWriteableData();
    void clear(GLContext &gl);
    void copy(GLContext &gl, GLBuffer &source);
    bool swap(GLBuffer &other);
    GLuint getReadOnlyBufferId(GLContext &gl);
    GLuint getReadWriteBufferId(GLContext &gl);
    void bindReadOnly(GLContext &gl, GLenum target);
    void bindIndexedRange(GLContext &gl, GLenum target, int index, int offset,
        int size, bool readonly);
    void unbind(GLContext &gl, GLenum target);
    void beginDownload(GLContext &context, bool checkModification);
    bool finishDownload();

private:
    void reload();
    void createBuffer(GLContext &gl);
    void upload(GLContext &gl);

    GLObject mBufferObject;
    bool mDownloaded{};
};

#endif // !defined(OPENGL_ENABLED)
