#pragma once

#include <QImage>
#include <QOpenGLTexture>
#include <QMetaType>
#include <memory>
#include <ktx.h>

class QOpenGLFunctions_3_3_Core;

class TextureData
{
public:
    bool isSharedWith(const TextureData &other) const;
    bool create(QOpenGLTexture::Target target,
        QOpenGLTexture::TextureFormat format,
        int width, int height, 
        int depth = 1, int layers = 1, int samples = 1,
        int levels = 0);
    bool load(const QString &fileName, bool flipVertically);
    bool loadQImage(QImage image, bool flipVertically);
    bool save(const QString &fileName, bool flipVertically) const;
    bool isNull() const;
    void clear();
    QImage toImage() const;
    bool isConvertibleToImage() const;

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
    bool flippedVertically() const { return mFlippedVertically; }
    uchar *getWriteonlyData(int level, int layer, int faceSlice);
    const uchar *getData(int level, int layer, int faceSlice) const;
    int getImageSize(int level) const;
    int getLevelSize(int level) const;
    void setPixelFormat(QOpenGLTexture::PixelFormat pixelFormat);
    bool upload(GLuint textureId, QOpenGLTexture::TextureFormat format =
        QOpenGLTexture::TextureFormat::NoFormat);
    bool upload(GLuint *textureId, QOpenGLTexture::TextureFormat format =
        QOpenGLTexture::TextureFormat::NoFormat);
    bool download(GLuint textureId);

    friend bool operator==(const TextureData &a, const TextureData &b);
    friend bool operator!=(const TextureData &a, const TextureData &b);

private:
    using GL = QOpenGLFunctions_3_3_Core;

    bool loadKtx(const QString &fileName, bool flipVertically);
    bool loadOpenImageIO(const QString &fileName, bool flipVertically);
    bool loadQImage(const QString &fileName, bool flipVertically);
    bool loadPfm(const QString &fileName, bool flipVertically);
    bool saveKtx(const QString &fileName, bool flipVertically) const;
    bool saveQImage(const QString &fileName, bool flipVertically) const;
    bool saveOpenImageIO(const QString &fileName, bool flipVertically) const;
    bool savePfm(const QString &fileName, bool flipVertically) const;
    void flipVertically();
    bool uploadMultisample(GL& gl, GLuint textureId,
        QOpenGLTexture::TextureFormat format);
    bool download(GL& gl, GLuint textureId);
    bool downloadCubemap(GL& gl, GLuint textureId);
    bool downloadMultisample(GL& gl, GLuint textureId);

    std::shared_ptr<ktxTexture> mKtxTexture;
    QOpenGLTexture::Target mTarget{ QOpenGLTexture::Target2D };
    int mSamples{ };
    bool mFlippedVertically{ };
};

enum class TextureSampleType
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
};

enum class TextureDataType
{
    Other,
    Int8,
    Int16,
    Int32,
    Uint8,
    Uint16,
    Uint32,
    Float16,
    Float32,
};

TextureSampleType getTextureSampleType(QOpenGLTexture::TextureFormat format);
TextureDataType getDataSampleType(QOpenGLTexture::TextureFormat format);
int getTextureDataSize(QOpenGLTexture::TextureFormat dataType);
int getTextureComponentCount(QOpenGLTexture::TextureFormat format);

Q_DECLARE_METATYPE(TextureData)
