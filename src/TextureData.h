#pragma once

#include <QImage>
#include <QOpenGLTexture>
#include <memory>
#include "ktx.h"

class QOpenGLFunctions_3_3_Core;

class TextureData
{
public:
    bool isSharedWith(const TextureData &other) const;
    bool create(QOpenGLTexture::Target target,
        QOpenGLTexture::TextureFormat format,
        int width, int height, int depth, int layers, int samples);
    bool load(const QString &fileName);
    bool save(const QString &fileName) const;
    bool isNull() const;
    void clear();
    QImage toImage() const;

    bool isArray() const;
    bool isCubemap() const;
    bool isCompressed() const;
    bool isMultisample() const;
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
    int samples() const { return mSamples; }
    uchar *getWriteonlyData(int level, int layer, int face);
    const uchar *getData(int level, int layer, int face) const;
    int getImageSize(int level) const;
    int getLevelSize(int level) const;
    bool upload(GLuint textureId, QOpenGLTexture::TextureFormat format =
        QOpenGLTexture::TextureFormat::NoFormat);
    bool upload(GLuint *textureId, QOpenGLTexture::TextureFormat format =
        QOpenGLTexture::TextureFormat::NoFormat);
    bool download(GLuint textureId);

    friend bool operator==(const TextureData &a, const TextureData &b);
    friend bool operator!=(const TextureData &a, const TextureData &b);

private:
    using GL = QOpenGLFunctions_3_3_Core;

    bool uploadMultisample(GL& gl, GLuint textureId,
        QOpenGLTexture::TextureFormat format);
    bool download(GL& gl, GLuint textureId);
    bool downloadCubemap(GL& gl, GLuint textureId);
    bool downloadMultisample(GL& gl, GLuint textureId);

    QOpenGLTexture::Target mTarget{ };
    int mSamples{ };
    std::shared_ptr<ktxTexture> mKtxTexture;
};

enum class TextureDataType
{
    Normalized,
    Normalized_sRGB,
    Float,
    Int8,
    Int16,
    Int32,
    Uint8,
    Uint16,
    Uint32,
    Uint_10_10_10_2,
    Compressed,
};

TextureDataType getTextureDataType(
    const QOpenGLTexture::TextureFormat &format);
int getTextureDataSize(TextureDataType dataType);
int getTextureComponentCount(
    QOpenGLTexture::TextureFormat format);
