#ifndef GLTEXTURE_H
#define GLTEXTURE_H

#include "PrepareContext.h"
#include "RenderContext.h"
#include <memory>
#include <QOpenGLTexture>

class GLTexture
{
public:
    GLTexture(const Texture &texture, PrepareContext &context);
    bool operator==(const GLTexture &rhs) const;

    ItemId itemId() const { return mItemId; }
    bool isDepthTexture() const;
    bool isSencilTexture() const;
    bool isDepthSencilTexture() const;
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    Texture::Target target() const { return mTarget; }
    Texture::Format format() const { return mFormat; }

    GLuint getReadOnlyTextureId(RenderContext &context);
    GLuint getReadWriteTextureId(RenderContext &context);
    QList<std::pair<QString, QImage>> getModifiedImages(RenderContext &context);

private:
    struct Image {
        int level;
        int layer;
        QOpenGLTexture::CubeMapFace face;
        QString fileName;
        QImage image;

        friend bool operator==(const Image &a, const Image &b) {
            return (std::tie(a.level, a.layer, a.face, a.fileName) ==
                    std::tie(b.level, b.layer, b.face, b.fileName));
        }
    };

    void load(MessageList &messages);
    void getImageDataFormat(QOpenGLTexture::PixelFormat *format,
        QOpenGLTexture::PixelType *dataType) const;
    void upload(RenderContext &context);
    void uploadImage(const Image &image);
    bool download(RenderContext &context);
    bool downloadImage(RenderContext &context, Image& image);

    ItemId mItemId{ };
    Texture::Target mTarget{ };
    Texture::Format mFormat{ };
    int mWidth{ };
    int mHeight{ };
    int mDepth{ };
    std::vector<Image> mImages;
    bool mSystemCopiesModified{ };
    bool mDeviceCopiesModified{ };
    std::unique_ptr<QOpenGLTexture> mTexture;
};

#endif // GLTEXTURE_H
