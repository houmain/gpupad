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
    bool isDepthSencilTexture() const;
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    Texture::Target target() const { return mTarget; }
    Texture::Format format() const { return mFormat; }

    GLuint getReadOnlyTextureId();
    GLuint getReadWriteTextureId();
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    QList<std::pair<QString, QImage>> getModifiedImages();

private:
    void getImageDataFormat(QOpenGLTexture::PixelFormat *format,
        QOpenGLTexture::PixelType *dataType) const;
    void load();
    void upload();
    void uploadImage(const Image &image);
    bool download();
    bool downloadImage(Image& image);

    QSet<ItemId> mUsedItems;
    QList<MessagePtr> mMessages;
    Texture::Target mTarget{ };
    Texture::Format mFormat{ };
    int mWidth{ };
    int mHeight{ };
    int mDepth{ };
    bool mFlipY{ };
    QList<Image> mImages;
    std::unique_ptr<QOpenGLTexture> mTexture;
    bool mSystemCopiesModified{ };
    bool mDeviceCopiesModified{ };
};

#endif // GLTEXTURE_H
