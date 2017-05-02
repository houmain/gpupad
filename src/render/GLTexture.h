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
    Texture::Target target() const { return mTarget; }
    Texture::Format format() const { return mFormat; }

    GLuint getReadOnlyTextureId(RenderContext &context);
    GLuint getReadWriteTextureId(RenderContext &context);
    QList<std::pair<QString, QImage>> getModifiedImages(RenderContext &context);

private:
    void load(MessageList &messages);
    void upload(RenderContext &context);
    bool download(RenderContext &context);

    ItemId mItemId{ };
    QString mFileName;
    QList<QImage> mImages;
    Texture::Target mTarget{ };
    Texture::Format mFormat{ };
    int mWidth{ };
    int mHeight{ };
    int mDepth{ };
    bool mSystemCopyModified{ };
    bool mDeviceCopyModified{ };
    std::unique_ptr<QOpenGLTexture> mTexture;
};

#endif // GLTEXTURE_H
