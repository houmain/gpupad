#ifndef GLTEXTURE_H
#define GLTEXTURE_H

#include "GLItem.h"
#include <QOpenGLTexture>

class GLTexture
{
public:
    explicit GLTexture(const Texture &texture);
    bool operator==(const GLTexture &rhs) const;

    ItemId itemId() const { return mItemId; }
    TextureKind kind() const { return mKind; }
    Texture::Target target() const { return mMultisampleTarget; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    int depth() const { return mDepth; }
    int layers() const { return mLayers; }
    Texture::Format format() const { return mFormat; }
    TextureData data() const { return mData; }
    void clear(QColor color, double depth, int stencil);
    void copy(GLTexture &source);
    void generateMipmaps();
    GLuint getReadOnlyTextureId();
    GLuint getReadWriteTextureId();
    bool download();
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    GLObject createFramebuffer(GLuint textureId, int level) const;
    void reload();
    void createTexture();
    void upload();
    void copyTexture(GLuint sourceTextureId,
        GLuint destTextureId, int level);

    ItemId mItemId{ };
    QString mFileName;
    bool mReadonly{ };
    Texture::Target mTarget{ };
    Texture::Format mFormat{ };
    int mWidth{ };
    int mHeight{ };
    int mDepth{ };
    int mLayers{ };
    int mSamples{ };
    TextureData mData;
    QSet<ItemId> mUsedItems;
    QList<MessagePtr> mMessages;
    TextureKind mKind{ };
    GLObject mTextureObject;
    Texture::Target mMultisampleTarget{ };
    GLObject mMultisampleTexture;
    bool mSystemCopyModified{ };
    bool mDeviceCopyModified{ };
};

#endif // GLTEXTURE_H
