#ifndef GLTEXTURE_H
#define GLTEXTURE_H

#include "GLItem.h"
#include <QOpenGLTexture>

class GLTexture
{
public:
    struct Image
    {
        ItemId itemId;
        int level;
        int layer;
        QOpenGLTexture::CubeMapFace face;
        QString fileName;
        QImage image;
    };

    explicit GLTexture(const Texture &texture);
    bool operator==(const GLTexture &rhs) const;

    TextureKind kind() const { return mKind; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    bool flipY() const { return mFlipY; }
    Texture::Target target() const { return mMultisampleTarget; }
    Texture::Format format() const { return mFormat; }

    void clear(QColor color, double depth, int stencil);
    void copy(GLTexture &source);
    void generateMipmaps();
    GLuint getReadOnlyTextureId();
    GLuint getReadWriteTextureId();
    bool canUpdatePreview() const;
    QMap<ItemId, QImage> getModifiedImages();
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    void getDataFormat(QOpenGLTexture::PixelFormat *format,
        QOpenGLTexture::PixelType *type) const;
    int getImageWidth(int level) const;
    int getImageHeight(int level) const;
    QImage::Format getImageFormat(QOpenGLTexture::PixelFormat format,
        QOpenGLTexture::PixelType type) const;
    GLObject createFramebuffer(GLuint textureId, int level) const;
    void reload();
    void createTexture();
    void upload();
    void uploadImage(const Image &image);
    bool download();
    bool downloadImage(Image &image);
    void resolveMultisampleTexture(QOpenGLTexture &source,
        QOpenGLTexture &dest, int level);

    QSet<ItemId> mUsedItems;
    QList<MessagePtr> mMessages;
    TextureKind mKind{ };
    Texture::Target mTarget{ };
    Texture::Format mFormat{ };
    int mWidth{ };
    int mHeight{ };
    int mDepth{ };
    int mLayers{ };
    int mSamples{ };
    bool mFlipY{ };
    QList<Image> mImages;
    std::unique_ptr<QOpenGLTexture> mTexture;
    Texture::Target mMultisampleTarget{ };
    std::unique_ptr<QOpenGLTexture> mMultisampleTexture;
    bool mSystemCopiesModified{ };
    bool mDeviceCopiesModified{ };
};

#endif // GLTEXTURE_H
