#pragma once

#include "GLItem.h"
#include "render/TextureBase.h"
#include <QOpenGLTexture>

class GLBuffer;

class GLTexture : public TextureBase
{
public:
    static bool upload(QOpenGLFunctions_3_3_Core &gl, const TextureData &data,
        QOpenGLTexture::Target target, int samples, GLuint *textureId);
    static bool download(QOpenGLFunctions_3_3_Core &gl, TextureData &data,
        QOpenGLTexture::Target target, GLuint textureId);

    GLTexture(const Texture &texture, GLRenderSession &renderSession);
    GLTexture(const Buffer &buffer, GLBuffer *textureBuffer,
        Texture::Format format, GLRenderSession &renderSession);
    bool operator==(const GLTexture &rhs) const;

    GLuint textureId() const { return mTextureObject; }
    GLuint64 obtainBindlessHandle();
    bool clear(std::array<double, 4> color, double depth, int stencil);
    bool copy(GLTexture &source);
    bool swap(GLTexture &other);
    bool updateMipmaps();
    GLuint getReadOnlyTextureId();
    GLuint getReadWriteTextureId();
    bool download();

private:
    void reload(bool forWriting);
    void createTexture();
    void upload();

    GLBuffer *mTextureBuffer{};
    GLObject mTextureObject;
    GLuint64 mBindlessHandle{};
};
