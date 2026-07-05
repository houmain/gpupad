#include "TextureData.h"
#include "session/Item.h"
#include <QImageReader>
#include <QFileInfo>
#include <QtEndian>
#include <cstring>
#include <limits>

#if defined(OPENIMAGEIO_ENABLED)
#  if defined(_MSC_VER)
#    pragma warning(disable : 4267)
#  endif
#  include <OpenImageIO/imageio.h>
#endif

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"

using TF = Texture::Format;
using TT = Texture::Target;

namespace {
    // TODO: remove when libKTX is fixed
    void fixFormat(ktxTexture1 &texture)
    {
        auto glGetFormatFromInternalFormat = [](const GLenum internalFormat) {
            switch (internalFormat) {
            case GL_R8UI:
                return GL_RED_INTEGER; // 1-component, 8-bit unsigned integer
            case GL_RG8UI:
                return GL_RG_INTEGER; // 2-component, 8-bit unsigned integer
            case GL_RGB8UI:
                return GL_RGB_INTEGER; // 3-component, 8-bit unsigned integer
            case GL_RGBA8UI:
                return GL_RGBA_INTEGER; // 4-component, 8-bit unsigned integer

            case GL_R8I:
                return GL_RED_INTEGER; // 1-component, 8-bit signed integer
            case GL_RG8I:
                return GL_RG_INTEGER; // 2-component, 8-bit signed integer
            case GL_RGB8I:
                return GL_RGB_INTEGER; // 3-component, 8-bit signed integer
            case GL_RGBA8I:
                return GL_RGBA_INTEGER; // 4-component, 8-bit signed integer

            case GL_R16UI:
                return GL_RED_INTEGER; // 1-component, 16-bit unsigned integer
            case GL_RG16UI:
                return GL_RG_INTEGER; // 2-component, 16-bit unsigned integer
            case GL_RGB16UI:
                return GL_RGB_INTEGER; // 3-component, 16-bit unsigned integer
            case GL_RGBA16UI:
                return GL_RGBA_INTEGER; // 4-component, 16-bit unsigned integer

            case GL_R16I:
                return GL_RED_INTEGER; // 1-component, 16-bit signed integer
            case GL_RG16I:
                return GL_RG_INTEGER; // 2-component, 16-bit signed integer
            case GL_RGB16I:
                return GL_RGB_INTEGER; // 3-component, 16-bit signed integer
            case GL_RGBA16I:
                return GL_RGBA_INTEGER; // 4-component, 16-bit signed integer

            case GL_R32UI:
                return GL_RED_INTEGER; // 1-component, 32-bit unsigned integer
            case GL_RG32UI:
                return GL_RG_INTEGER; // 2-component, 32-bit unsigned integer
            case GL_RGB32UI:
                return GL_RGB_INTEGER; // 3-component, 32-bit unsigned integer
            case GL_RGBA32UI:
                return GL_RGBA_INTEGER; // 4-component, 32-bit unsigned integer

            case GL_R32I:
                return GL_RED_INTEGER; // 1-component, 32-bit signed integer
            case GL_RG32I:
                return GL_RG_INTEGER; // 2-component, 32-bit signed integer
            case GL_RGB32I:
                return GL_RGB_INTEGER; // 3-component, 32-bit signed integer
            case GL_RGBA32I:
                return GL_RGBA_INTEGER; // 4-component, 32-bit signed integer

            case GL_RGB10_A2UI:
                return GL_RGBA_INTEGER; // 4-component 10:10:10:2,  unsigned
            default: return GL_NONE;
            }
        };
        if (auto format =
                glGetFormatFromInternalFormat(texture.glInternalformat))
            texture.glFormat = format;
    }

    QImage::Format getNextNativeImageFormat(QImage::Format format)
    {
        switch (format) {
        case QImage::Format_Mono:
        case QImage::Format_MonoLSB:
        case QImage::Format_Alpha8:
        case QImage::Format_Grayscale8:            return QImage::Format_Grayscale8;
        case QImage::Format_BGR30:
        case QImage::Format_A2BGR30_Premultiplied:
        case QImage::Format_RGB30:
        case QImage::Format_A2RGB30_Premultiplied:
        case QImage::Format_RGBX64:
        case QImage::Format_RGBA64:
        case QImage::Format_RGBA64_Premultiplied:  return QImage::Format_RGBA64;
        case QImage::Format_Grayscale16:           return QImage::Format_Grayscale16;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 2, 0))
        case QImage::Format_RGBX16FPx4:
        case QImage::Format_RGBA16FPx4:
        case QImage::Format_RGBA16FPx4_Premultiplied:
            return QImage::Format_RGBA16FPx4;
        case QImage::Format_RGBX32FPx4:
        case QImage::Format_RGBA32FPx4:
        case QImage::Format_RGBA32FPx4_Premultiplied:
            return QImage::Format_RGBA32FPx4;
#endif
        default: return QImage::Format_RGBA8888;
        }
    }

    Texture::Format getTextureFormat(QImage::Format format)
    {
        switch (format) {
        case QImage::Format_RGB30:       return TF::RGB10A2;
        case QImage::Format_RGB888:      return TF::RGB8_UNorm;
        case QImage::Format_RGBA8888:    return TF::RGBA8_UNorm;
        case QImage::Format_RGBA64:      return TF::RGBA16_UNorm;
        case QImage::Format_Grayscale8:  return TF::R8_UNorm;
        case QImage::Format_Grayscale16: return TF::R16_UNorm;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 2, 0))
        case QImage::Format_RGBA16FPx4: return TF::RGBA16F;
        case QImage::Format_RGBA32FPx4: return TF::RGBA32F;
#endif
        default: return TF::NoFormat;
        }
    }

    QImage::Format getImageFormat(uint32_t format, uint32_t type)
    {
        enum PixelFormat {
            NoSourceFormat = 0, // GL_NONE
            Red = 0x1903, // GL_RED
            RG = 0x8227, // GL_RG
            RGB = 0x1907, // GL_RGB
            BGR = 0x80E0, // GL_BGR
            RGBA = 0x1908, // GL_RGBA
            BGRA = 0x80E1, // GL_BGRA
            Red_Integer = 0x8D94, // GL_RED_INTEGER
            RG_Integer = 0x8228, // GL_RG_INTEGER
            RGB_Integer = 0x8D98, // GL_RGB_INTEGER
            BGR_Integer = 0x8D9A, // GL_BGR_INTEGER
            RGBA_Integer = 0x8D99, // GL_RGBA_INTEGER
            BGRA_Integer = 0x8D9B, // GL_BGRA_INTEGER
            Stencil = 0x1901, // GL_STENCIL_INDEX
            Depth = 0x1902, // GL_DEPTH_COMPONENT
            DepthStencil = 0x84F9, // GL_DEPTH_STENCIL
            Alpha = 0x1906, // GL_ALPHA
            Luminance = 0x1909, // GL_LUMINANCE
            LuminanceAlpha = 0x190A // GL_LUMINANCE_ALPHA
        };

        enum PixelType {
            NoPixelType = 0, // GL_NONE
            Int8 = 0x1400, // GL_BYTE
            UInt8 = 0x1401, // GL_UNSIGNED_BYTE
            Int16 = 0x1402, // GL_SHORT
            UInt16 = 0x1403, // GL_UNSIGNED_SHORT
            Int32 = 0x1404, // GL_INT
            UInt32 = 0x1405, // GL_UNSIGNED_INT
            Float16 = 0x140B, // GL_HALF_FLOAT
            Float16OES = 0x8D61, // GL_HALF_FLOAT_OES
            Float32 = 0x1406, // GL_FLOAT
            UInt32_RGB9_E5 = 0x8C3E, // GL_UNSIGNED_INT_5_9_9_9_REV
            UInt32_RG11B10F = 0x8C3B, // GL_UNSIGNED_INT_10F_11F_11F_REV
            UInt8_RG3B2 = 0x8032, // GL_UNSIGNED_BYTE_3_3_2
            UInt8_RG3B2_Rev = 0x8362, // GL_UNSIGNED_BYTE_2_3_3_REV
            UInt16_RGB5A1 = 0x8034, // GL_UNSIGNED_SHORT_5_5_5_1
            UInt16_RGB5A1_Rev = 0x8366, // GL_UNSIGNED_SHORT_1_5_5_5_REV
            UInt16_R5G6B5 = 0x8363, // GL_UNSIGNED_SHORT_5_6_5
            UInt16_R5G6B5_Rev = 0x8364, // GL_UNSIGNED_SHORT_5_6_5_REV
            UInt16_RGBA4 = 0x8033, // GL_UNSIGNED_SHORT_4_4_4_4
            UInt16_RGBA4_Rev = 0x8365, // GL_UNSIGNED_SHORT_4_4_4_4_REV
            UInt32_RGBA8 = 0x8035, // GL_UNSIGNED_INT_8_8_8_8
            UInt32_RGBA8_Rev = 0x8367, // GL_UNSIGNED_INT_8_8_8_8_REV
            UInt32_RGB10A2 = 0x8036, // GL_UNSIGNED_INT_10_10_10_2
            UInt32_RGB10A2_Rev = 0x8368, // GL_UNSIGNED_INT_2_10_10_10_REV
            UInt32_D24S8 = 0x84FA, // GL_UNSIGNED_INT_24_8
            Float32_D32_UInt32_S8_X24 =
                0x8DAD // GL_FLOAT_32_UNSIGNED_INT_24_8_REV
        };

        switch (type) {
        case PixelType::Int8:
        case PixelType::UInt8:
            switch (format) {
            case PixelFormat::Red:
            case PixelFormat::Red_Integer:
            case PixelFormat::Depth:
            case PixelFormat::Stencil:     return QImage::Format_Grayscale8;

            case PixelFormat::RGB:
            case PixelFormat::RGB_Integer: return QImage::Format_RGB888;

            case PixelFormat::RGBA:
            case PixelFormat::RGBA_Integer: return QImage::Format_RGBA8888;

            default: return QImage::Format_Invalid;
            }

        case PixelType::Int16:
        case PixelType::UInt16:
            switch (format) {
            case PixelFormat::RGBA:
            case PixelFormat::RGBA_Integer: return QImage::Format_RGBA64;

            default: return QImage::Format_Invalid;
            }

        case PixelType::UInt32_D24S8: return QImage::Format_RGBA8888;

        default: return QImage::Format_Invalid;
        }
    }

    ktx_uint32_t getLevelCount(const ktxTextureCreateInfo &info)
    {
        auto dimension =
            std::max(std::max(info.baseWidth, info.baseHeight), info.baseDepth);
        auto levels = ktx_uint32_t{};
        for (; dimension; dimension >>= 1)
            ++levels;
        return levels;
    }

    bool isDepthStencilFormat(Texture::Format format)
    {
        switch (format) {
        case TF::D16:
        case TF::D24:
        case TF::D24S8:
        case TF::D32:
        case TF::D32F:
        case TF::D32FS8X24:
        case TF::S8:        return true;
        default:            return false;
        }
    }

    bool canGenerateMipmaps(Texture::Target target, Texture::Format format)
    {
        switch (target) {
        case TT::Target1D:
        case TT::Target1DArray:
        case TT::Target2D:
        case TT::Target2DArray:
        case TT::Target3D:
        case TT::TargetCubeMap:
        case TT::TargetCubeMapArray: break;
        default:                     return false;
        }

        if (isDepthStencilFormat(format))
            return false;

        const auto sampleType = getTextureSampleType(format);
        return (sampleType == TextureSampleType::Normalized
            || sampleType == TextureSampleType::Normalized_sRGB
            || sampleType == TextureSampleType::Float);
    }

    template <typename Source, typename Dest, int Shift>
    void convert(const Source *source, Dest *dest, int pixels,
        int sourceComponents, int destComponents)
    {
        for (auto i = 0; i < pixels; ++i) {
            for (auto c = 0; c < destComponents; ++c) {
                const auto v = (c < sourceComponents ? source[c]
                        : c < 3                      ? Source{}
                                : std::numeric_limits<Source>::max());
                if constexpr (Shift < 0) {
                    dest[c] = static_cast<Dest>(v >> -Shift);
                } else {
                    dest[c] = static_cast<Dest>(v << Shift);
                }
            }
            source += sourceComponents;
            dest += destComponents;
        }
    }

    bool convertPlane(const uchar *source, Texture::Format sourceFormat,
        uchar *dest, Texture::Format destFormat, int pixels)
    {
        if (!source || !dest)
            return false;

        const auto sourceDataType = getTextureDataType(sourceFormat);
        const auto destDataType = getTextureDataType(destFormat);
        const auto sourceComponents = getTextureComponentCount(sourceFormat);
        const auto destComponents = getTextureComponentCount(destFormat);

#define ADD(SOURCE_TYPE, SOURCE, DEST_TYPE, DEST, SHIFT)                       \
    if (sourceDataType == TextureDataType::SOURCE_TYPE                         \
        && destDataType == TextureDataType::DEST_TYPE) {                       \
        convert<SOURCE, DEST, SHIFT>(reinterpret_cast<const SOURCE *>(source), \
            reinterpret_cast<DEST *>(dest), pixels, sourceComponents,          \
            destComponents);                                                   \
        return true;                                                           \
    }
        ADD(Uint8, uint8_t, Uint8, uint8_t, 0)
        ADD(Uint8, uint8_t, Uint16, uint16_t, 8)
        ADD(Uint8, uint8_t, Uint32, uint32_t, 16)
        ADD(Uint8, uint8_t, Int8, int8_t, -1)
        ADD(Uint8, uint8_t, Int16, int16_t, 7)
        ADD(Uint8, uint8_t, Int32, int32_t, 15)

        ADD(Uint16, uint16_t, Uint8, uint8_t, -8)
        ADD(Uint16, uint16_t, Uint16, uint16_t, 0)
        ADD(Uint16, uint16_t, Uint32, uint32_t, 8)
        ADD(Uint16, uint16_t, Int8, int8_t, -9)
        ADD(Uint16, uint16_t, Int16, int16_t, -1)
        ADD(Uint16, uint16_t, Int32, int32_t, 7)

        ADD(Uint32, uint32_t, Uint8, uint8_t, -16)
        ADD(Uint32, uint32_t, Uint16, uint16_t, -8)
        ADD(Uint32, uint32_t, Uint32, uint32_t, 0)
        ADD(Uint32, uint32_t, Int8, int8_t, -17)
        ADD(Uint32, uint32_t, Int16, int16_t, -9)
        ADD(Uint32, uint32_t, Int32, int32_t, -1)
#undef ADD
        return false;
    }

    bool resizePlane(const uchar *source, Texture::Format format, uchar *dest,
        int sourceWidth, int sourceHeight, int sourceStride, int destWidth,
        int destHeight, int destStride)
    {
        if (!source || !dest || destWidth <= 0 || destHeight <= 0)
            return false;

        auto pixelLayout = stbir_pixel_layout{};
        switch (getTextureComponentCount(format)) {
        case 1:  pixelLayout = STBIR_1CHANNEL; break;
        case 2:  pixelLayout = STBIR_2CHANNEL; break;
        case 3:  pixelLayout = STBIR_RGB; break;
        case 4:  pixelLayout = STBIR_RGBA; break;
        default: return {};
        }

        auto dataType = stbir_datatype{};
        switch (getTextureDataType(format)) {
        case TextureDataType::Packed:
        case TextureDataType::Compressed:
        case TextureDataType::Int8:
        case TextureDataType::Int16:
        case TextureDataType::Int32:      return {};
        case TextureDataType::Uint8:      dataType = STBIR_TYPE_UINT8_SRGB; break;
        case TextureDataType::Uint16:     dataType = STBIR_TYPE_UINT16; break;
        case TextureDataType::Uint32:     return {};
        case TextureDataType::Float16:    dataType = STBIR_TYPE_FLOAT; break;
        case TextureDataType::Float32:    dataType = STBIR_TYPE_HALF_FLOAT; break;
        }
        const auto filter = STBIR_FILTER_DEFAULT;
        const auto edgeMode = STBIR_EDGE_CLAMP;
        return stbir_resize(source, sourceWidth, sourceHeight, sourceStride,
            dest, destWidth, destHeight, destStride, pixelLayout, dataType,
            edgeMode, filter);
    }

    QImage flipImage(QImage &&image)
    {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 9, 0))
        image = image.flipped(Qt::Vertical);
#else
        image = std::move(image).mirrored();
#endif
        return image;
    }
} // namespace

bool isMultisampleTarget(Texture::Target target)
{
    return (target == TT::Target2DMultisample
        || target == TT::Target2DMultisampleArray);
}

bool isCubemapTarget(Texture::Target target)
{
    return (target == TT::TargetCubeMap || target == TT::TargetCubeMapArray);
}

TextureSampleType getTextureSampleType(Texture::Format format)
{
    switch (format) {
    case TF::SRGB8:
    case TF::SRGB8_Alpha8: return TextureSampleType::Normalized_sRGB;

    case TF::R16F:
    case TF::RG16F:
    case TF::RGB16F:
    case TF::RGBA16F:
    case TF::R32F:
    case TF::RG32F:
    case TF::RGB32F:
    case TF::RGBA32F:
    case TF::RGB9E5:
    case TF::RG11B10F: return TextureSampleType::Float;

    case TF::R8U:
    case TF::RG8U:
    case TF::RGB8U:
    case TF::RGBA8U:
    case TF::S8:     return TextureSampleType::Uint8;

    case TF::R16U:
    case TF::RG16U:
    case TF::RGB16U:
    case TF::RGBA16U: return TextureSampleType::Uint16;

    case TF::R32U:
    case TF::RG32U:
    case TF::RGB32U:
    case TF::RGBA32U: return TextureSampleType::Uint32;

    case TF::R8I:
    case TF::RG8I:
    case TF::RGB8I:
    case TF::RGBA8I: return TextureSampleType::Int8;

    case TF::R16I:
    case TF::RG16I:
    case TF::RGB16I:
    case TF::RGBA16I: return TextureSampleType::Int16;

    case TF::R32I:
    case TF::RG32I:
    case TF::RGB32I:
    case TF::RGBA32I: return TextureSampleType::Int32;

    case TF::RGB10A2: return TextureSampleType::Uint_10_10_10_2;

    default: return TextureSampleType::Normalized;
    }
}

TextureDataType getTextureDataType(Texture::Format format)
{
    switch (format) {
    case TF::R8_UNorm:
    case TF::RG8_UNorm:
    case TF::RGB8_UNorm:
    case TF::RGBA8_UNorm:
    case TF::SRGB8:
    case TF::SRGB8_Alpha8:
    case TF::R8U:
    case TF::RG8U:
    case TF::RGB8U:
    case TF::RGBA8U:
    case TF::S8:           return TextureDataType::Uint8;

    case TF::R16_UNorm:
    case TF::RG16_UNorm:
    case TF::RGB16_UNorm:
    case TF::RGBA16_UNorm:
    case TF::R16U:
    case TF::RG16U:
    case TF::RGB16U:
    case TF::RGBA16U:
    case TF::D16:          return TextureDataType::Uint16;

    case TF::R8_SNorm:
    case TF::RG8_SNorm:
    case TF::RGB8_SNorm:
    case TF::RGBA8_SNorm:
    case TF::R8I:
    case TF::RG8I:
    case TF::RGB8I:
    case TF::RGBA8I:      return TextureDataType::Int8;

    case TF::R16_SNorm:
    case TF::RG16_SNorm:
    case TF::RGB16_SNorm:
    case TF::RGBA16_SNorm:
    case TF::R16I:
    case TF::RG16I:
    case TF::RGB16I:
    case TF::RGBA16I:      return TextureDataType::Int16;

    case TF::R32U:
    case TF::RG32U:
    case TF::RGB32U:
    case TF::RGBA32U:
    case TF::D32:     return TextureDataType::Uint32;

    case TF::R32I:
    case TF::RG32I:
    case TF::RGB32I:
    case TF::RGBA32I: return TextureDataType::Int32;

    case TF::R16F:
    case TF::RG16F:
    case TF::RGB16F:
    case TF::RGBA16F: return TextureDataType::Float16;

    case TF::R32F:
    case TF::RG32F:
    case TF::RGB32F:
    case TF::RGBA32F:
    case TF::D32F:    return TextureDataType::Float32;

    case TF::RGB9E5:
    case TF::RG11B10F:
    case TF::RG3B2:
    case TF::R5G6B5:
    case TF::RGB5A1:
    case TF::RGBA4:
    case TF::RGB10A2:  return TextureDataType::Packed;

    default: return TextureDataType::Compressed;
    }
}

int getTextureDataSize(Texture::Format format)
{
    switch (getTextureDataType(format)) {
    case TextureDataType::Int8:       return 1;
    case TextureDataType::Int16:      return 2;
    case TextureDataType::Int32:      return 4;
    case TextureDataType::Uint8:      return 1;
    case TextureDataType::Uint16:     return 2;
    case TextureDataType::Uint32:     return 4;
    case TextureDataType::Float16:    return 2;
    case TextureDataType::Float32:    return 4;
    case TextureDataType::Packed:     return 0;
    case TextureDataType::Compressed: return 0;
    }
    return 0;
}

int getTextureComponentCount(Texture::Format format)
{
    switch (format) {
    case TF::R8_UNorm:
    case TF::R8_SNorm:
    case TF::R16_UNorm:
    case TF::R16_SNorm:
    case TF::R8U:
    case TF::R8I:
    case TF::R16U:
    case TF::R16I:
    case TF::R32U:
    case TF::R32I:
    case TF::R16F:
    case TF::R32F:
    case TF::D16:
    case TF::D24:
    case TF::D32:
    case TF::D32F:
    case TF::D24S8:
    case TF::D32FS8X24:
    case TF::S8:
    case TF::R_ATI1N_UNorm:
    case TF::R_ATI1N_SNorm:
    case TF::R11_EAC_UNorm:
    case TF::R11_EAC_SNorm: return 1;

    case TF::RG8_UNorm:
    case TF::RG8_SNorm:
    case TF::RG16_UNorm:
    case TF::RG16_SNorm:
    case TF::RG8U:
    case TF::RG8I:
    case TF::RG16U:
    case TF::RG16I:
    case TF::RG32U:
    case TF::RG32I:
    case TF::RG16F:
    case TF::RG32F:
    case TF::RG_ATI2N_UNorm:
    case TF::RG_ATI2N_SNorm:
    case TF::RGB_BP_UNSIGNED_FLOAT:
    case TF::RGB_BP_SIGNED_FLOAT:
    case TF::RG11_EAC_UNorm:
    case TF::RG11_EAC_SNorm:        return 2;

    case TF::RGB8_UNorm:
    case TF::RGB8_SNorm:
    case TF::RGB16_UNorm:
    case TF::RGB16_SNorm:
    case TF::RGB8U:
    case TF::RGB8I:
    case TF::RGB16U:
    case TF::RGB16I:
    case TF::RGB32U:
    case TF::RGB32I:
    case TF::RGB16F:
    case TF::RGB32F:
    case TF::SRGB8:
    case TF::SRGB_DXT1:
    case TF::RGB9E5:
    case TF::RG11B10F:
    case TF::RG3B2:
    case TF::R5G6B5:
    case TF::RGB_DXT1:
    case TF::RGB8_ETC2:
    case TF::SRGB8_ETC2:  return 3;

    default: return 4;
    }
}

bool operator==(const TextureData &a, const TextureData &b)
{
    if (a.format() != b.format() || a.width() != b.width()
        || a.height() != b.height() || a.depth() != b.depth()
        || a.levels() != b.levels() || a.layers() != b.layers()
        || a.faces() != b.faces())
        return false;

    if (a.isSharedWith(b))
        return true;

    for (auto level = 0; level < a.levels(); ++level)
        for (auto layer = 0; layer < a.layers(); ++layer)
            for (auto face = 0; face < a.faces(); ++face) {
                const auto da = a.getData(level, layer, face);
                const auto db = b.getData(level, layer, face);
                if (da == db)
                    continue;
                const auto sa = a.getImageSize(level);
                const auto sb = b.getImageSize(level);
                if (sa != sb
                    || std::memcmp(da, db, static_cast<size_t>(sa)) != 0)
                    return false;
            }
    return true;
}

bool TextureData::isSharedWith(const TextureData &other) const
{
    return (mKtxTexture == other.mKtxTexture);
}

bool operator!=(const TextureData &a, const TextureData &b)
{
    return !(a == b);
}

bool TextureData::create(Texture::Target target, Texture::Format format,
    int width, int height, int depth, int layers, int levels)
{
    if (width <= 0 || height <= 0 || depth <= 0 || layers <= 0)
        return false;

    auto createInfo = ktxTextureCreateInfo{};
    createInfo.glInternalformat = format;
    createInfo.baseWidth = static_cast<ktx_uint32_t>(width);
    createInfo.baseHeight = 1;
    createInfo.baseDepth = 1;
    createInfo.numLayers = 1;
    createInfo.numFaces = 1;
    createInfo.isArray = KTX_FALSE;

    Q_ASSERT(!isMultisampleTarget(target));

    switch (target) {
    case TT::Target1DArray:
        createInfo.isArray = KTX_TRUE;
        createInfo.numLayers = static_cast<ktx_uint32_t>(layers);
        [[fallthrough]];
    case TT::Target1D: createInfo.numDimensions = 1; break;

    case TT::Target2DArray:
        createInfo.isArray = KTX_TRUE;
        createInfo.numLayers = static_cast<ktx_uint32_t>(layers);
        [[fallthrough]];
    case TT::Target2D:
    case TT::TargetRectangle:
        createInfo.numDimensions = 2;
        createInfo.baseHeight = static_cast<ktx_uint32_t>(height);
        break;

    case TT::Target3D:
        createInfo.numDimensions = 3;
        createInfo.baseHeight = static_cast<ktx_uint32_t>(height);
        createInfo.baseDepth = static_cast<ktx_uint32_t>(depth);
        break;

    case TT::TargetCubeMapArray:
        createInfo.isArray = KTX_TRUE;
        createInfo.numLayers = static_cast<ktx_uint32_t>(layers);
        createInfo.numLayers *= 6;
        [[fallthrough]];
    case TT::TargetCubeMap:
        createInfo.baseHeight = createInfo.baseWidth;
        createInfo.numDimensions = 2;
        createInfo.numFaces = 6;
        break;

    default: return false;
    }

    createInfo.numLevels = (levels > 0           ? levels
            : canGenerateMipmaps(target, format) ? getLevelCount(createInfo)
                                                 : 1);

    auto texture = std::add_pointer_t<ktxTexture1>{};
    if (ktxTexture1_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE,
            &texture)
        == KTX_SUCCESS) {
        fixFormat(*texture);
        mKtxTexture.reset(texture,
            [](ktxTexture1 *tex) { ktxTexture_Destroy(ktxTexture(tex)); });
        return true;
    }
    return false;
}

TextureData TextureData::convert(Texture::Format format) const
{
    if (format == this->format())
        return *this;

    auto copy = TextureData();
    if (!copy.create(getTarget(), format, width(), height(), depth(), layers(),
            levels()))
        return {};

    // only write first level, it will trigger the mipmap generation
    const auto level = 0;
    for (auto layer = 0; layer < layers(); ++layer)
        for (auto faceSlice = 0; faceSlice < depth() * faces(); ++faceSlice)
            if (!convertPlane(getData(level, layer, faceSlice), this->format(),
                    copy.getWriteonlyData(level, layer, faceSlice), format,
                    getLevelWidth(level) * getLevelHeight(level)))
                return {};

    copy.setFlippedVertically(flippedVertically());
    return copy;
}

TextureData TextureData::resize(int width, int height, int depth,
    int layers) const
{
    if (depth != this->depth() || layers != this->layers())
        return {};
    if (width == this->width() && height == this->height())
        return *this;

    auto copy = TextureData();
    if (!copy.create(getTarget(), format(), width, height, depth, layers,
            (levels() == 1 ? 1 : 0)))
        return {};

    const auto level = 0;
    for (auto layer = 0; layer < layers; ++layer)
        for (auto faceSlice = 0; faceSlice < depth * faces(); ++faceSlice)
            if (!resizePlane(getData(level, layer, faceSlice), this->format(),
                    copy.getWriteonlyData(level, layer, faceSlice),
                    getLevelWidth(level), getLevelHeight(level),
                    getLevelStride(level), copy.getLevelWidth(level),
                    copy.getLevelHeight(level), copy.getLevelStride(level)))
                return {};

    copy.setFlippedVertically(flippedVertically());
    return copy;
}

TextureData TextureData::convert(Texture::Format format, int width, int height,
    int depth, int layers) const
{
    auto converted = convert(format);
    if (converted.isNull())
        return {};
    return converted.resize(width, height, depth, layers);
}

bool TextureData::loadKtx(const QString &fileName, bool flipVertically)
{
    auto f = std::fopen(qUtf8Printable(fileName), "rb");
    if (!f)
        return false;
    auto guard = qScopeGuard([&]() { std::fclose(f); });

    auto texture = std::add_pointer_t<ktxTexture1>{};
    if (ktxTexture1_CreateFromStdioStream(f,
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture)
        != KTX_SUCCESS)
        return false;

    mKtxTexture.reset(texture,
        [](ktxTexture1 *tex) { ktxTexture_Destroy(ktxTexture(tex)); });

    if (flipVertically)
        this->flipVertically();

    return true;
}

bool TextureData::loadOpenImageIO(const QString &fileName, bool flipVertically)
{
#if !defined(OPENIMAGEIO_ENABLED)
    return false;
#else // OPENIMAGEIO_ENABLED
    using namespace OIIO;
    auto input = ImageInput::open(qUtf8Printable(fileName));
    if (!input) {
        OIIO::geterror();
        return false;
    }
    using F = Texture::Format;
    const ImageSpec &spec = input->spec();

    const auto channel = [&](auto c1, auto c2, auto c3, auto c4) {
        switch (spec.nchannels) {
        case 1: return c1;
        case 2: return c2;
        case 3: return c3;
        case 4: return c4;
        }
        return F::NoFormat;
    };
    const auto format = [&]() {
        switch (spec.format.basetype) {
        case TypeDesc::UINT8:
            return channel(F::R8_UNorm, F::RG8_UNorm, F::RGB8_UNorm,
                F::RGBA8_UNorm);
        case TypeDesc::INT8:
            return channel(F::R8_SNorm, F::RG8_SNorm, F::RGB8_SNorm,
                F::RGBA8_SNorm);
        case TypeDesc::UINT16:
            return channel(F::R16_UNorm, F::RG16_UNorm, F::RGB16_UNorm,
                F::RGBA16_UNorm);
        case TypeDesc::INT16:
            return channel(F::R16_SNorm, F::RG16_SNorm, F::RGB16_SNorm,
                F::RGBA16_SNorm);
        case TypeDesc::UINT32:
            return channel(F::R32U, F::RG32U, F::RGB32U, F::RGBA32U);
        case TypeDesc::INT32:
            return channel(F::R32I, F::RG32I, F::RGB32I, F::RGBA32I);
        //case TypeDesc::UINT64: return channel(F::R64U, F::RG64U, F::RGB64U, F::RGBA64U);
        //case TypeDesc::INT64: return channel(F::R64I, F::RG64I, F::RGB64I, F::RGBA64I);
        case TypeDesc::HALF:
            return channel(F::R16F, F::RG16F, F::RGB16F, F::RGBA16F);
        case TypeDesc::FLOAT:
            return channel(F::R32F, F::RG32F, F::RGB32F, F::RGBA32F);
            //case TypeDesc::TOUBLE: return channel(F::R64F, F::RG64F, F::RGB64F, F::RGBA64F);
        }
        return F::NoFormat;
    }();
    const auto target = (spec.depth > 1 ? TT::Target3D : TT::Target2D);

    if (!create(target, format, spec.width, spec.height, spec.depth, 1))
        return false;

    const auto stride = getLevelStride(0);

    // TODO: load all layers/levels
    if (!input->read_image(0, 0, 0, -1, spec.format, getWriteonlyData(0, 0, 0),
            OIIO::AutoStride, stride))
        return false;

    if (flipVertically)
        this->flipVertically();

    return true;
#endif // OPENIMAGEIO_ENABLED
}

bool TextureData::loadQImage(const QString &fileName, bool flipVertically)
{
    auto imageReader = QImageReader(fileName);
    imageReader.setAutoTransform(true);
    imageReader.setAllocationLimit(0);

    auto image = QImage();
    if (!imageReader.read(&image))
        return false;

    return loadQImage(std::move(image), flipVertically);
}

bool TextureData::loadQImage(QImage image, bool flipVertically)
{
    image = std::move(image).convertToFormat(
        getNextNativeImageFormat(image.format()));

    if (flipVertically)
        image = flipImage(std::move(image));

    if (!create(TT::Target2D, getTextureFormat(image.format()), image.width(),
            image.height(), 1, 1))
        return false;

    if (static_cast<int>(image.sizeInBytes()) != getImageSize(0))
        return false;

    std::memcpy(getWriteonlyData(0, 0, 0), image.constBits(),
        static_cast<size_t>(getImageSize(0)));

    mFlippedVertically = flipVertically;
    return true;
}

bool TextureData::loadPfm(const QString &fileName, bool flipVertically)
{
    auto f = std::fopen(qUtf8Printable(fileName), "rb");
    if (!f)
        return false;
    auto guard = qScopeGuard([&]() { std::fclose(f); });

    auto c0 = char{};
    auto c1 = char{};
    auto read = static_cast<size_t>(std::fscanf(f, "%c%c\n", &c0, &c1));
    if (c0 != 'P' || read != 2)
        return false;

    auto channels = 3;
    if (c1 != 'F') {
        if (c1 != 'f')
            return false;
        channels = 1;
    }
    auto width = 0;
    auto height = 0;
    auto scale = 1.0;
    if (fscanf(f, "%d %d\n", &width, &height) != 2
        || std::fscanf(f, "%lf\n", &scale) != 1 || std::fscanf(f, "\n") != 0)
        return false;

    auto endianness = QSysInfo::BigEndian;
    if (scale < 0) {
        endianness = QSysInfo::LittleEndian;
        scale = -scale;
    }
    const auto format = (channels == 3 ? TF::RGB32F : TF::R32F);
    if (!create(TT::Target2D, format, width, height, 1, 1))
        return false;
    const auto size = static_cast<size_t>(getImageSize(0));
    const auto data = getWriteonlyData(0, 0, 0);

    if (endianness == QSysInfo::ByteOrder) {
        read = std::fread(data, 1, size, f);
    } else {
        auto buffer = QByteArray(getImageSize(0), 0);
        read = std::fread(buffer.data(), 1, size, f);
        if (endianness == QSysInfo::LittleEndian) {
            qFromLittleEndian<float>(buffer.data(), size / 4, data);
        } else {
            qFromBigEndian<float>(buffer.data(), size / 4, data);
        }
    }
    return (read == size);
}

bool TextureData::load(const QString &fileName, bool flipVertically)
{
    return loadKtx(fileName, flipVertically)
        || loadPfm(fileName, flipVertically)
        || loadOpenImageIO(fileName, flipVertically)
        || loadQImage(fileName, flipVertically);
}

bool TextureData::saveKtx(const QString &fileName, bool flipVertically) const
{
    if (!fileName.endsWith(".ktx", Qt::CaseInsensitive))
        return false;

    return (ktxTexture_WriteToNamedFile(ktxTexture(mKtxTexture.get()),
                qUtf8Printable(fileName))
        == KTX_SUCCESS);
}

bool TextureData::savePfm(const QString &fileName, bool flipVertically) const
{
    if (!fileName.endsWith(".pfm", Qt::CaseInsensitive))
        return false;

    // TODO
    return false;
}

bool TextureData::saveOpenImageIO(const QString &fileName,
    bool flipVertically) const
{
#if !defined(OPENIMAGEIO_ENABLED)
    return false;
#else // OPENIMAGEIO_ENABLED
    using namespace OIIO;

    const auto typeDesc = [&]() {
        switch (getTextureDataType(format())) {
        case TextureDataType::Uint8:      return TypeDesc::UINT8;
        case TextureDataType::Int8:       return TypeDesc::INT8;
        case TextureDataType::Uint16:     return TypeDesc::UINT16;
        case TextureDataType::Int16:      return TypeDesc::INT16;
        case TextureDataType::Uint32:     return TypeDesc::UINT32;
        case TextureDataType::Int32:      return TypeDesc::INT32;
        case TextureDataType::Float16:    return TypeDesc::HALF;
        case TextureDataType::Float32:    return TypeDesc::FLOAT;
        case TextureDataType::Packed:     return TypeDesc::NONE;
        case TextureDataType::Compressed: return TypeDesc::NONE;
        }
        return TypeDesc::NONE;
    }();
    if (typeDesc == TypeDesc::NONE)
        return false;

    auto output = ImageOutput::create(qUtf8Printable(fileName));
    const auto guard = qScopeGuard([]() { OIIO::geterror(); });
    if (!output)
        return false;

    const auto channelCount = getTextureComponentCount(format());
    const auto spec = ImageSpec(width(), height(), channelCount, typeDesc);
    if (!output->open(qUtf8Printable(fileName), spec))
        return false;
    if (!output->write_image(typeDesc, getData(0, 0, 0)))
        return false;
    return true;
#endif // OPENIMAGEIO_ENABLED
}

bool TextureData::saveQImage(const QString &fileName, bool flipVertically) const
{
    auto image = toImage();
    if (image.isNull())
        return false;

    if (flipVertically)
        image = flipImage(std::move(image));

    const auto hasExtension = !QFileInfo(fileName).suffix().isEmpty();
    return image.save(fileName, hasExtension ? nullptr : "PNG");
}

bool TextureData::save(const QString &fileName, bool flipVertically) const
{
    return saveKtx(fileName, flipVertically)
        || savePfm(fileName, flipVertically)
        || saveOpenImageIO(fileName, flipVertically)
        || saveQImage(fileName, flipVertically);
}

bool TextureData::isNull() const
{
    return (mKtxTexture == nullptr);
}

bool TextureData::isConvertibleToImage() const
{
    if (depth() > 1 || layers() > 1 || isCubemap())
        return false;

    return getImageFormat(pixelFormat(), pixelType()) != QImage::Format_Invalid;
}

QImage TextureData::toImage() const
{
    if (!isConvertibleToImage())
        return {};

    const auto imageFormat = getImageFormat(pixelFormat(), pixelType());
    if (imageFormat == QImage::Format_Invalid)
        return {};

    auto image = QImage(width(), height(), imageFormat);
    if (static_cast<int>(image.sizeInBytes()) != getImageSize(0))
        return {};

    std::memcpy(image.bits(), getData(0, 0, 0),
        static_cast<size_t>(getImageSize(0)));

    return image;
}

bool TextureData::isArray() const
{
    return (!isNull() && mKtxTexture->isArray);
}

bool TextureData::isCubemap() const
{
    return (!isNull() && mKtxTexture->isCubemap);
}

bool TextureData::isCompressed() const
{
    return (!isNull() && mKtxTexture->isCompressed);
}

int TextureData::dimensions() const
{
    return (isNull() ? 0 : static_cast<int>(mKtxTexture->numDimensions));
}

Texture::Target TextureData::getTarget(int samples) const
{
    Q_ASSERT(mKtxTexture);
    if (!mKtxTexture)
        return {};
    const auto &texture = *mKtxTexture;
    if (texture.isCubemap)
        return (texture.isArray ? TT::TargetCubeMapArray : TT::TargetCubeMap);
    switch (texture.numDimensions) {
    case 1: return (texture.isArray ? TT::Target1DArray : TT::Target1D);
    case 2:
        if (samples > 1)
            return (texture.isArray ? TT::Target2DMultisampleArray
                                    : TT::Target2DMultisample);
        return (texture.isArray ? TT::Target2DArray : TT::Target2D);
    case 3: return TT::Target3D;
    }
    return {};
}

Texture::Format TextureData::format() const
{
    return (isNull()
            ? TF::NoFormat
            : static_cast<Texture::Format>(mKtxTexture->glInternalformat));
}

uint32_t TextureData::pixelFormat() const
{
    return (isNull() ? 0 : mKtxTexture->glFormat);
}

uint32_t TextureData::pixelType() const
{
    return (isNull() ? 0 : mKtxTexture->glType);
}

int TextureData::getLevelWidth(int level) const
{
    return (isNull()
            ? 0
            : std::max(static_cast<int>(mKtxTexture->baseWidth) >> level, 1));
}

int TextureData::getLevelHeight(int level) const
{
    return (isNull()
            ? 0
            : std::max(static_cast<int>(mKtxTexture->baseHeight) >> level, 1));
}

int TextureData::getLevelDepth(int level) const
{
    return (isNull()
            ? 0
            : std::max(static_cast<int>(mKtxTexture->baseDepth) >> level, 1));
}

int TextureData::getLevelStride(int level) const
{
    if (auto height = getLevelHeight(level))
        return getImageSize(level) / height;
    return 0;
}

int TextureData::levels() const
{
    return (isNull() ? 0 : static_cast<int>(mKtxTexture->numLevels));
}

int TextureData::layers() const
{
    if (isNull())
        return 0;

    if (getTarget() == TT::TargetCubeMapArray)
        return static_cast<int>(mKtxTexture->numLayers / 6);

    return static_cast<int>(mKtxTexture->numLayers);
}

int TextureData::faces() const
{
    return (isNull() ? 0 : static_cast<int>(mKtxTexture->numFaces));
}

uchar *TextureData::getWriteonlyData()
{
    return getWriteonlyData(0, 0, 0);
}

const uchar *TextureData::getData() const
{
    return getData(0, 0, 0);
}

int TextureData::getDataSize() const
{
    return static_cast<int>(
        ktxTexture_GetDataSize(ktxTexture(mKtxTexture.get())));
}

uchar *TextureData::getWriteonlyData(int level, int layer, int face)
{
    if (isNull())
        return nullptr;

    if (mKtxTexture.use_count() > 1)
        create(getTarget(), format(), width(), height(), depth(), layers(),
            levels());

    // generate mipmaps on next upload when level 0 is written
    mKtxTexture->generateMipmaps =
        (level == 0 && levels() > 1 ? KTX_TRUE : KTX_FALSE);

    return const_cast<uchar *>(
        static_cast<const TextureData *>(this)->getData(level, layer, face));
}

const uchar *TextureData::getData(int level, int layer, int faceSlice) const
{
    if (isNull())
        return nullptr;

    auto offset = ktx_size_t{};
    if (ktxTexture_GetImageOffset(ktxTexture(mKtxTexture.get()),
            static_cast<ktx_uint32_t>(level), static_cast<ktx_uint32_t>(layer),
            static_cast<ktx_uint32_t>(faceSlice), &offset)
        == KTX_SUCCESS)
        return ktxTexture_GetData(ktxTexture(mKtxTexture.get())) + offset;

    return nullptr;
}

size_t TextureData::getOffset(int level, int layer, int faceSlice) const
{
    return std::distance(getData(), getData(level, layer, faceSlice));
}

int TextureData::getImageSize(int level) const
{
    if (isNull())
        return 0;
    return static_cast<int>(ktxTexture_GetImageSize(
        ktxTexture(mKtxTexture.get()), static_cast<ktx_uint32_t>(level)));
}

int TextureData::getSlicesSize(int level) const
{
    return getImageSize(level) * getLevelDepth(level);
}

int TextureData::getLevelSize(int level) const
{
    return getSlicesSize(level) * layers() * faces();
}

void TextureData::clear()
{
    if (isNull())
        return;

    const auto level = 0;
    for (auto layer = 0; layer < layers(); ++layer)
        for (auto face = 0; face < faces(); ++face)
            std::memset(getWriteonlyData(level, layer, face), 0x00,
                static_cast<size_t>(getImageSize(level)));
}

void TextureData::flipVertically()
{
    const auto pitch = getLevelStride(0);
    const auto data = getWriteonlyData(0, 0, 0);
    auto buffer = std::vector<std::byte>(pitch);
    auto low = data;
    auto high = &data[(height() - 1) * pitch];
    for (; low < high; low += pitch, high -= pitch) {
        std::memcpy(buffer.data(), low, pitch);
        std::memcpy(low, high, pitch);
        std::memcpy(high, buffer.data(), pitch);
    }
    mFlippedVertically = !mFlippedVertically;
}

#if defined(OPENGL_ENABLED)
bool TextureData::uploadGL(GLuint *textureId) const
{
    if (isNull() || !textureId)
        return false;

    auto error = GLenum{};
    auto target = static_cast<GLenum>(getTarget());
    const auto result = ktxTexture_GLUpload(ktxTexture(mKtxTexture.get()),
        textureId, &target, &error);

    Q_ASSERT(glGetError() == GL_NO_ERROR);
    return (result == KTX_SUCCESS);
}
#endif // defined(OPENGL_ENABLED)

#if defined(VULKAN_ENABLED)
bool TextureData::uploadVK(ktxVulkanDeviceInfo *vdi,
    ktxVulkanTexture *vkTexture, VkImageUsageFlags usageFlags,
    VkImageLayout finalLayout) const
{
    if (isNull() || !vkTexture || !vdi)
        return false;

    const auto texture = ktxTexture(mKtxTexture.get());
    const auto numLevels = texture->numLevels;
    const auto dataSize = texture->dataSize;
    const auto restoreTexture = qScopeGuard([&] {
        texture->numLevels = numLevels;
        texture->dataSize = dataSize;
    });

    // KTX's Vulkan mipmap path expects only the base level to be present.
    if (texture->generateMipmaps && texture->numLevels > 1) {
        texture->numLevels = 1;
        texture->dataSize = static_cast<ktx_size_t>(getLevelSize(0));
    }

    const auto result = ktxTexture_VkUploadEx(texture,
        vdi, vkTexture, VK_IMAGE_TILING_OPTIMAL, usageFlags, finalLayout);

    return (result == KTX_SUCCESS);
}
#endif // defined(VULKAN_ENABLED)
