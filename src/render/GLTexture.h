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

    bool isDepthTexture() const;
    bool isSencilTexture() const;
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    Texture::Target target() const { return mTarget; }
    Texture::Format format() const { return mFormat; }

    void clear(QVariantList value);
    void generateMipmaps();
    GLuint getReadOnlyTextureId();
    GLuint getReadWriteTextureId();
    QList<std::pair<QString, QImage>> getModifiedImages();
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    void getImageDataFormat(QOpenGLTexture::PixelFormat *format,
        QOpenGLTexture::PixelType *dataType) const;
    int getImageWidth(int level) const;
    int getImageHeight(int level) const;
    GLObject createFramebuffer(GLuint textureId, int level) const;
    void load();
    void upload();
    void uploadImage(const Image &image);
    bool download();
    bool downloadImage(Image& image);
    void resolveMultisampleTexture(QOpenGLTexture &source,
        QOpenGLTexture &dest, int level);

    QSet<ItemId> mUsedItems;
    QList<MessagePtr> mMessages;
    Texture::Target mTarget{ };
    Texture::Format mFormat{ };
    int mWidth{ };
    int mHeight{ };
    int mDepth{ };
    int mSamples{ };
    bool mFlipY{ };
    QList<Image> mImages;
    std::unique_ptr<QOpenGLTexture> mTexture;
    std::unique_ptr<QOpenGLTexture> mResolveTexture;
    bool mSystemCopiesModified{ };
    bool mDeviceCopiesModified{ };
};

#endif // GLTEXTURE_H
