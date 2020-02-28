#include "ImageData.h"
#include <cstring>

namespace {
    QImage::Format getNextNativeImageFormat(QImage::Format format)
    {
        switch (format) {
            case QImage::Format_Mono:
            case QImage::Format_MonoLSB:
            case QImage::Format_Alpha8:
                return QImage::Format_Grayscale8;
            case QImage::Format_RGBX64:
                return QImage::Format_RGBA64;
            default:
                return QImage::Format_RGBA8888;
        }
    }

    QOpenGLTexture::TextureFormat getTextureFormat(QImage::Format format)
    {
        switch (format) {
            case QImage::Format_RGB30:
                return QOpenGLTexture::RGB10A2;
            case QImage::Format_RGB888:
                return QOpenGLTexture::RGB8_UNorm;
            case QImage::Format_RGBA8888:
                return QOpenGLTexture::RGBA8_UNorm;
            case QImage::Format_RGBA64:
                return QOpenGLTexture::RGBA16_UNorm;
            case QImage::Format_Grayscale8:
                return QOpenGLTexture::R8_UNorm;
            case QImage::Format_Grayscale16:
                return QOpenGLTexture::R16_UNorm;
            default:
                return getTextureFormat(getNextNativeImageFormat(format));
        }
    }

    QImage::Format getImageFormat(
        QOpenGLTexture::PixelFormat format,
        QOpenGLTexture::PixelType type)
    {
        switch (type) {
            case QOpenGLTexture::Int8:
            case QOpenGLTexture::UInt8:
                switch (format) {
                    case QOpenGLTexture::Red:
                    case QOpenGLTexture::Red_Integer:
                    case QOpenGLTexture::Depth:
                    case QOpenGLTexture::Stencil:
                        return QImage::Format_Grayscale8;

                    case QOpenGLTexture::RGB:
                    case QOpenGLTexture::RGB_Integer:
                        return QImage::Format_RGB888;

                    case QOpenGLTexture::RGBA:
                    case QOpenGLTexture::RGBA_Integer:
                        return QImage::Format_RGBA8888;

                    default:
                        return QImage::Format_Invalid;
                }

            case QOpenGLTexture::Int16:
            case QOpenGLTexture::UInt16:
                switch (format) {
                    case QOpenGLTexture::RGBA:
                    case QOpenGLTexture::RGBA_Integer:
                        return QImage::Format_RGBA64;

                    default:
                        return QImage::Format_Invalid;
                }

            case QOpenGLTexture::UInt32_D24S8:
                return QImage::Format_RGBA8888;

            default:
                return QImage::Format_Invalid;
        }
    }
} // namespace

bool operator==(const ImageData &a, const ImageData &b) 
{
    if (a.width() != b.width() ||
        a.height() != b.height() ||
        a.depth() != b.depth() ||
        a.levels() != b.levels() ||
        a.layers() != b.layers() ||
        a.faces() != b.faces())
        return false;
    for (auto level = 0; level < a.levels(); ++level)
        for (auto layer = 0; layer < a.layers(); ++layer)
            for (auto face = 0; face < a.faces(); ++face) {
                const auto da = a.getData(level, layer, face);
                const auto db = b.getData(level, layer, face);
                if (da == db)
                    continue;
                const auto sa = a.getLevelSize(level);
                const auto sb = b.getLevelSize(level);
                if (sa != sb || std::memcmp(da, db, sa))
                    return false;
            }
    return true;
}

bool operator!=(const ImageData &a, const ImageData &b)
{
    return !(a == b);
}

bool ImageData::create(int width, int height,
    QOpenGLTexture::TextureFormat format) 
{
    // TODO: generalize
    auto createInfo = ktxTextureCreateInfo{ };
    createInfo.glInternalformat = format;
    createInfo.baseWidth = static_cast<ktx_uint32_t>(width);
    createInfo.baseHeight = static_cast<ktx_uint32_t>(height);
    createInfo.baseDepth = 1;
    createInfo.numDimensions = 2;
    createInfo.numLevels = 1;
    createInfo.numLayers = 1;
    createInfo.numFaces = 1;
    createInfo.isArray = KTX_FALSE;

    ktxTexture* texture{ };
    if (ktxTexture_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE,
        &texture) == KTX_SUCCESS) {
      mKtxTexture.reset(texture, &ktxTexture_Destroy);
      return true;
    }
    return false;
}

bool ImageData::load(const QString &fileName) 
{
    ktxTexture* texture{ };
    if (ktxTexture_CreateFromNamedFile(fileName.toUtf8().constData(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) == KTX_SUCCESS) {
        mKtxTexture.reset(texture, &ktxTexture_Destroy);
        return true;
    }

    QImage image;
    if (!image.load(fileName))
        return false;
    if (!create(image.width(), image.height(), getTextureFormat(image.format())))
        return false;
    if (static_cast<size_t>(image.sizeInBytes()) != getLevelSize(0))
        return false;
    std::memcpy(getWriteonlyData(0, 0, 0), image.constBits(), getLevelSize(0));
    return true;
}

bool ImageData::save(const QString &fileName) const 
{
    if (fileName.toLower().endsWith(".ktx") &&
        ktxTexture_WriteToNamedFile(mKtxTexture.get(), 
        fileName.toUtf8().constData()))
      return true;

    return toImage().save(fileName);
}

bool ImageData::isNull() const 
{
    return (mKtxTexture == nullptr);
}

QImage ImageData::toImage() const 
{
    const auto imageFormat = getImageFormat(pixelFormat(), pixelType());
    if (imageFormat == QImage::Format_Invalid)
        return { };
    auto image = QImage(width(), height(), imageFormat);
    if (static_cast<size_t>(image.sizeInBytes()) != getLevelSize(0))
        return { };
    std::memcpy(image.bits(), getData(0, 0, 0), getLevelSize(0));
    return image;
}

bool ImageData::isArray() const
{
    return (!isNull() && mKtxTexture->isArray);
}

bool ImageData::isCubemap() const
{
    return (!isNull() && mKtxTexture->isCubemap);
}

bool ImageData::isCompressed() const
{
    return (!isNull() && mKtxTexture->isCompressed);
}

int ImageData::dimensions() const
{
    return (isNull() ? 0 :
        static_cast<int>(mKtxTexture->numDimensions));
}

QOpenGLTexture::TextureFormat ImageData::format() const
{
    return (isNull() ? QOpenGLTexture::TextureFormat::NoFormat : 
        static_cast<QOpenGLTexture::TextureFormat>(mKtxTexture->glInternalformat));
}

QOpenGLTexture::PixelFormat ImageData::pixelFormat() const
{
    return (isNull() ? QOpenGLTexture::PixelFormat::NoSourceFormat :
        static_cast<QOpenGLTexture::PixelFormat>(mKtxTexture->glFormat));
}

QOpenGLTexture::PixelType ImageData::pixelType() const
{
    return (isNull() ? QOpenGLTexture::PixelType::NoPixelType :
        static_cast<QOpenGLTexture::PixelType>(mKtxTexture->glType));
}

int ImageData::getLevelWidth(int level) const
{
    return (isNull() ? 0 :
        std::max(static_cast<int>(mKtxTexture->baseWidth) >> level, 1));
}

int ImageData::getLevelHeight(int level) const
{
    return (isNull() ? 0 :
        std::max(static_cast<int>(mKtxTexture->baseHeight) >> level, 1));
}

int ImageData::getLevelDepth(int level) const
{
    return (isNull() ? 0 :
        std::max(static_cast<int>(mKtxTexture->baseDepth) >> level, 1));
}

int ImageData::levels() const
{
    return (isNull() ? 0 : static_cast<int>(mKtxTexture->numLevels));
}

int ImageData::layers() const
{
    return (isNull() ? 0 : static_cast<int>(mKtxTexture->numLayers));
}

int ImageData::faces() const
{
    return (isNull() ? 0 : static_cast<int>(mKtxTexture->numFaces));
}

uchar *ImageData::getWriteonlyData(int level, int layer, int face)
{
    if (isNull())
        return nullptr;

    if (mKtxTexture.use_count() > 1)
        create(width(), height(), format());

    return const_cast<uchar*>(
        static_cast<const ImageData*>(this)->getData(level, layer, face));
}

const uchar *ImageData::getData(int level, int layer, int face) const
{
    if (isNull())
        return nullptr;
    
    auto offset = ktx_size_t{ };
    if (ktxTexture_GetImageOffset(mKtxTexture.get(), 
            static_cast<ktx_uint32_t>(level),
            static_cast<ktx_uint32_t>(layer),
            static_cast<ktx_uint32_t>(face), &offset) == KTX_SUCCESS)
        return ktxTexture_GetData(mKtxTexture.get()) + offset;

    return nullptr;
}

int ImageData::getLevelSize(int level) const
{
    return (isNull() ? 0 :
      static_cast<int>(ktxTexture_GetImageSize(mKtxTexture.get(),
          static_cast<ktx_uint32_t>(level))));
}
