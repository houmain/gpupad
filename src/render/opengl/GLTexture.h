#ifndef GLTEXTURE_H
#define GLTEXTURE_H

#include "GLItem.h"
#include <QOpenGLTexture>

class GLBuffer;
class ScriptEngine;

class GLTexture
{
public:
    static bool upload(QOpenGLFunctions_3_3_Core &gl,
        const TextureData &data, QOpenGLTexture::Target target, 
        int samples, GLuint* textureId);
    static bool download(QOpenGLFunctions_3_3_Core &gl,
        TextureData &data, QOpenGLTexture::Target target, 
        GLuint textureId);

    GLTexture(const Texture &texture, ScriptEngine &scriptEngine);
    GLTexture(const Buffer &buffer, GLBuffer *textureBuffer, 
        Texture::Format format, ScriptEngine &scriptEngine);
    bool operator==(const GLTexture &rhs) const;

    ItemId itemId() const { return mItemId; }
    const QString &fileName() const { return mFileName; }
    TextureKind kind() const { return mKind; }
    Texture::Target target() const { return mTarget; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    int depth() const { return mDepth; }
    int samples() const { return mSamples; }
    int layers() const { return mLayers; }
    Texture::Format format() const { return mFormat; }
    TextureData data() const { return mData; }
    GLuint textureId() const { return mTextureObject; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

    bool clear(std::array<double, 4> color, double depth, int stencil);
    bool copy(GLTexture &source);
    bool swap(GLTexture &other);
    bool updateMipmaps();
    GLuint getReadOnlyTextureId();
    GLuint getReadWriteTextureId();
    bool deviceCopyModified() const { return mDeviceCopyModified; }
    bool download();

private:
    void reload(bool forWriting);
    void createTexture();
    void upload();

    ItemId mItemId{ };
    MessagePtrSet mMessages;
    QString mFileName;
    bool mFlipVertically{ };
    GLBuffer *mTextureBuffer{ };
    Texture::Target mTarget{ };
    Texture::Format mFormat{ };
    int mWidth{ };
    int mHeight{ };
    int mDepth{ };
    int mLayers{ };
    int mSamples{ };
    TextureData mData;
    bool mDataWritten{ };
    QSet<ItemId> mUsedItems;
    TextureKind mKind{ };
    GLObject mTextureObject;
    bool mSystemCopyModified{ };
    bool mDeviceCopyModified{ };
    bool mMipmapsInvalidated{ };
};

#endif // GLTEXTURE_H
