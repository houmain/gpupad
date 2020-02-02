#pragma once

#include <QImage>
#include <memory>
#include <QOpenGLTexture>
#include "ktx.h"

class ImageData
{
public:
    bool create(int width, int height,
        QOpenGLTexture::TextureFormat format);
    ImageData convert(int width, int height,
        QOpenGLTexture::TextureFormat format,
        bool flipY) const;

    bool isNull() const;
    bool load(const QString &fileName);
    bool save(const QString &fileName) const;

    const QImage &image() const;
    int width() const;
    int height() const;

    QOpenGLTexture::TextureFormat format() const { return mFormat; }
    QOpenGLTexture::PixelFormat pixelFormat() const { return mPixelFormat; }
    QOpenGLTexture::PixelType pixelType() const { return mPixelType; }
    uchar *bits();
    const uchar *constBits() const;

private:
    QOpenGLTexture::TextureFormat mFormat{ };
    QOpenGLTexture::PixelFormat mPixelFormat{ };
    QOpenGLTexture::PixelType mPixelType{ };
    QImage mQImage;
    std::shared_ptr<const ktxTexture> mKtxTexture;
};

