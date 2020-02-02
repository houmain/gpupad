#include "ImageData.h"
#include <cstring>

namespace {
    void getDataFormat(
        QOpenGLTexture::TextureFormat textureFormat,
        QOpenGLTexture::PixelFormat *format,
        QOpenGLTexture::PixelType *type)
    {
        switch (textureFormat) {
            case QOpenGLTexture::R8U:
            case QOpenGLTexture::R16U:
            case QOpenGLTexture::R32U:
                *format = QOpenGLTexture::Red_Integer;
                *type = QOpenGLTexture::UInt8;
                break;

            case QOpenGLTexture::RG8U:
            case QOpenGLTexture::RGB8U:
            case QOpenGLTexture::RG16U:
            case QOpenGLTexture::RGB16U:
            case QOpenGLTexture::RG32U:
            case QOpenGLTexture::RGB32U:
                *format = QOpenGLTexture::RGB_Integer;
                *type = QOpenGLTexture::UInt8;
                break;

            case QOpenGLTexture::RGBA8U:
                *format = QOpenGLTexture::RGBA_Integer;
                *type = QOpenGLTexture::UInt8;
                break;

            case QOpenGLTexture::RGB10A2:
            case QOpenGLTexture::RGBA16U:
            case QOpenGLTexture::RGBA32U:
                *format = QOpenGLTexture::RGBA_Integer;
                *type = QOpenGLTexture::UInt16;
                break;

            case QOpenGLTexture::R8I:
            case QOpenGLTexture::R16I:
            case QOpenGLTexture::R32I:
                *format = QOpenGLTexture::Red_Integer;
                *type = QOpenGLTexture::Int8;
                break;

            case QOpenGLTexture::RG8I:
            case QOpenGLTexture::RGB8I:
            case QOpenGLTexture::RG16I:
            case QOpenGLTexture::RGB16I:
            case QOpenGLTexture::RG32I:
            case QOpenGLTexture::RGB32I:
                *format = QOpenGLTexture::RGB_Integer;
                *type = QOpenGLTexture::Int8;
                break;

            case QOpenGLTexture::RGBA8I:
                *format = QOpenGLTexture::RGBA_Integer;
                *type = QOpenGLTexture::Int8;
                break;

            case QOpenGLTexture::RGBA16I:
            case QOpenGLTexture::RGBA32I:
                *format = QOpenGLTexture::RGBA_Integer;
                *type = QOpenGLTexture::Int16;
                break;

            case QOpenGLTexture::D16:
            case QOpenGLTexture::D24:
            case QOpenGLTexture::D32:
            case QOpenGLTexture::D32F:
                *format = QOpenGLTexture::Depth;
                *type = QOpenGLTexture::UInt8;
                break;

            case QOpenGLTexture::D24S8:
                *format = QOpenGLTexture::DepthStencil;
                *type = QOpenGLTexture::UInt32_D24S8;
                break;

            case QOpenGLTexture::D32FS8X24:
                *format = QOpenGLTexture::DepthStencil;
                *type = QOpenGLTexture::Float32_D32_UInt32_S8_X24;
                break;

            case QOpenGLTexture::S8:
                *format = QOpenGLTexture::Stencil;
                *type = QOpenGLTexture::UInt8;
                break;

            case QOpenGLTexture::RGBA16_SNorm:
            case QOpenGLTexture::RGBA16_UNorm:
            case QOpenGLTexture::RGBA16F:
            case QOpenGLTexture::RGBA32F:
                *format = QOpenGLTexture::RGBA;
                *type = QOpenGLTexture::UInt16;
                break;

            default:
                *format = QOpenGLTexture::RGBA;
                *type = QOpenGLTexture::UInt8;
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

bool ImageData::create(int width, int height,
    QOpenGLTexture::TextureFormat format)
{
    mFormat = format;
    getDataFormat(format, &mPixelFormat, &mPixelType);
    const auto imageFormat = getImageFormat(mPixelFormat, mPixelType);
    if (imageFormat == QImage::Format_Invalid)
        return false;
    if (mQImage.width() != width ||
        mQImage.height() != height ||
        mQImage.format() != imageFormat) {
        mQImage = QImage(width, height, imageFormat);
        mQImage.fill(Qt::black);
    }
    return true;
}

ImageData ImageData::convert(int width, int height,
    QOpenGLTexture::TextureFormat format, bool flipY) const
{
    auto pixelFormat = QOpenGLTexture::PixelFormat{ };
    auto pixelType = QOpenGLTexture::PixelType{ };
    getDataFormat(format, &pixelFormat, &pixelType);
    const auto imageFormat = getImageFormat(pixelFormat, pixelType);
    if (imageFormat == QImage::Format_Invalid)
        return { };
    auto result = ImageData{ };
    result.mQImage =
        mQImage.convertToFormat(imageFormat).scaled(width, height);
    if (flipY)
        result.mQImage = result.mQImage.mirrored();
    result.mFormat = format;
    result.mPixelFormat = pixelFormat;
    result.mPixelType = pixelType;
    return result;
}

bool ImageData::isNull() const
{
    return mQImage.isNull();
}

bool ImageData::load(const QString &fileName)
{
    auto texture = std::add_pointer_t<ktxTexture>{ };
    if (ktxTexture_CreateFromNamedFile(fileName.toUtf8().constData(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) == KTX_SUCCESS) {
        mKtxTexture.reset(texture, &ktxTexture_Destroy);
        mFormat = static_cast<QOpenGLTexture::TextureFormat>(
            mKtxTexture->glInternalformat);
        mPixelFormat = static_cast<QOpenGLTexture::PixelFormat>(mKtxTexture->glFormat);
        mPixelType = static_cast<QOpenGLTexture::PixelType>(mKtxTexture->glType);
        mQImage = QImage(static_cast<int>(mKtxTexture->baseWidth),
                         static_cast<int>(mKtxTexture->baseHeight),
                         getImageFormat(mPixelFormat, mPixelType));
        if (mQImage.sizeInBytes() == static_cast<qsizetype>(mKtxTexture->dataSize)) {
            std::memcpy(mQImage.bits(), mKtxTexture->pData, mKtxTexture->dataSize);
            return true;
        }
    }
    return mQImage.load(fileName);
}

bool ImageData::save(const QString &fileName) const
{
    return mQImage.save(fileName);
}

const QImage &ImageData::image() const
{
    return mQImage;
}

int ImageData::width() const
{
    return mQImage.width();
}

int ImageData::height() const
{
    return mQImage.height();
}

uchar *ImageData::bits()
{
    return mQImage.bits();
}

const uchar *ImageData::constBits() const
{
    return mQImage.constBits();
}
