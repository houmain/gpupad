#pragma once

#include <QImage>
#include <QOpenGLTexture>
#include <memory>
#include "ktx.h"

class ImageData
{
public:
    bool create(int width, int height,
        QOpenGLTexture::TextureFormat format);
    bool load(const QString &fileName);
    bool save(const QString &fileName) const;
    bool isNull() const;
    QImage toImage() const;

    bool isArray() const;
    bool isCubemap() const;
    bool isCompressed() const;
    int dimensions() const;
    QOpenGLTexture::TextureFormat format() const;
    QOpenGLTexture::PixelFormat pixelFormat() const;
    QOpenGLTexture::PixelType pixelType() const;
    int width() const { return getLevelWidth(0); }
    int height() const { return getLevelHeight(0); }
    int depth() const { return getLevelDepth(0); }
    int getLevelWidth(int level) const;
    int getLevelHeight(int level) const;
    int getLevelDepth(int level) const;
    int levels() const;
    int layers() const;
    int faces() const;
    uchar *getWriteonlyData(int level, int layer, int face);
    const uchar *getData(int level, int layer, int face) const;
    size_t getLevelSize(int level) const;

    friend bool operator==(const ImageData &a, const ImageData &b);
    friend bool operator!=(const ImageData &a, const ImageData &b);

private:
    std::shared_ptr<ktxTexture> mKtxTexture;
};

