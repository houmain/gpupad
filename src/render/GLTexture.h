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
    const QString &fileName() const { return mFileName; }
    TextureKind kind() const { return mKind; }
    Texture::Target target() const { return mTarget; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    int depth() const { return mDepth; }
    int layers() const { return mLayers; }
    Texture::Format format() const { return mFormat; }
    TextureData data() const { return mData; }
    GLuint textureId() const { return mTextureObject; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

    bool clear(std::array<double, 4> color, double depth, int stencil);
    bool copy(GLTexture &source);
    bool updateMipmaps();
    GLuint getReadOnlyTextureId();
    GLuint getReadWriteTextureId();
    bool download();

private:
    GLObject createFramebuffer(GLuint textureId, int level) const;
    void reload();
    void createTexture();
    void upload();
    bool copyTexture(GLuint sourceTextureId,
        GLuint destTextureId, int level);

    ItemId mItemId{ };
    QString mFileName;
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
    bool mSystemCopyModified{ };
    bool mDeviceCopyModified{ };
    bool mMipmapsInvalidated{ };
};

#endif // GLTEXTURE_H
