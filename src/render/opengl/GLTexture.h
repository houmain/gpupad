#pragma once

#include "GLContext.h"
#include "render/TextureBase.h"
#include <QOpenGLTexture>

class GLBuffer;

class GLTexture : public TextureBase
{
public:
    static bool upload(GLContext &gl, const TextureData &data,
        Texture::Target target, int samples, GLuint *textureId);
    static bool download(GLContext &gl, TextureData &data,
        Texture::Target target, GLuint textureId);

    GLTexture(const Texture &texture, GLRenderSession &renderSession);
    GLTexture(const Buffer &buffer, GLBuffer *textureBuffer,
        Texture::Format format, GLRenderSession &renderSession);
    bool operator==(const GLTexture &rhs) const;

    void boundAsSampler() { }
    void boundAsImage() { }

    GLuint64 obtainBindlessHandle(GLContext &gl);
    bool clear(GLContext &gl, std::array<double, 4> color, double depth,
        int stencil);
    bool copy(GLContext &gl, GLTexture &source);
    bool swap(GLTexture &other);
    bool updateMipmaps(GLContext &context);
    GLuint getReadOnlyTextureId(GLContext &gl);
    GLuint getReadWriteTextureId(GLContext &gl);
    void beginDownload(GLContext &context);
    bool finishDownload();
    ShareHandle getShareHandle();

private:
    void reload(GLContext &gl, bool forWriting);
    void createTexture(GLContext &gl);
    void upload(GLContext &gl);

    GLBuffer *mTextureBuffer{};
    GLObject mTextureObject;
    GLuint64 mBindlessHandle{};
    bool mDownloaded{};
};
