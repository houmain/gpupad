#pragma once

#include <QImage>
#include <QOpenGLTexture>
#include <memory>
#include "ktx.h"

class TextureData
{
public:
    bool create(QOpenGLTexture::Target target,
        QOpenGLTexture::TextureFormat format,
        int width, int height, int depth, int layers);
    bool load(const QString &fileName);
    bool save(const QString &fileName) const;
    bool isNull() const;
    void clear();
    QImage toImage() const;

    bool isArray() const;
    bool isCubemap() const;
    bool isCompressed() const;
    int dimensions() const;
    QOpenGLTexture::Target target() const { return mTarget; }
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
    int getLevelSize(int level) const;
    bool upload(GLuint textureId);
    bool upload(GLuint *textureId);
    bool download(GLuint textureId);

    friend bool operator==(const TextureData &a, const TextureData &b);
    friend bool operator!=(const TextureData &a, const TextureData &b);

private:
    QOpenGLTexture::Target mTarget{ };
    std::shared_ptr<ktxTexture> mKtxTexture;
};

