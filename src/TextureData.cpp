#include "TextureData.h"
#include "session/Item.h"
#include <QImageReader>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QScopeGuard>
#include <QtEndian>
#include <cstring>
#include <limits>

#if defined(OpenImageIO_FOUND)
#  if defined(_MSC_VER)
#    pragma warning(disable : 4267)
#  endif
#  include <OpenImageIO/imageio.h>
#endif

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
        case QImage::Format_Grayscale16: return QImage::Format_Grayscale16;
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

    QOpenGLTexture::TextureFormat getTextureFormat(QImage::Format format)
    {
        switch (format) {
        case QImage::Format_RGB30:      return QOpenGLTexture::RGB10A2;
        case QImage::Format_RGB888:     return QOpenGLTexture::RGB8_UNorm;
        case QImage::Format_RGBA8888:   return QOpenGLTexture::RGBA8_UNorm;
        case QImage::Format_RGBA64:     return QOpenGLTexture::RGBA16_UNorm;
        case QImage::Format_Grayscale8: return QOpenGLTexture::R8_UNorm;
        case QImage::Format_Grayscale16: return QOpenGLTexture::R16_UNorm;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 2, 0))
        case QImage::Format_RGBA16FPx4: return QOpenGLTexture::RGBA16F;
        case QImage::Format_RGBA32FPx4: return QOpenGLTexture::RGBA32F;
#endif
        default: return QOpenGLTexture::NoFormat;
        }
    }

    QImage::Format getImageFormat(QOpenGLTexture::PixelFormat format,
        QOpenGLTexture::PixelType type)
    {
        switch (type) {
        case QOpenGLTexture::Int8:
        case QOpenGLTexture::UInt8:
            switch (format) {
            case QOpenGLTexture::Red:
            case QOpenGLTexture::Red_Integer:
            case QOpenGLTexture::Depth:
            case QOpenGLTexture::Stencil:     return QImage::Format_Grayscale8;

            case QOpenGLTexture::RGB:
            case QOpenGLTexture::RGB_Integer: return QImage::Format_RGB888;

            case QOpenGLTexture::RGBA:
            case QOpenGLTexture::RGBA_Integer: return QImage::Format_RGBA8888;

            default: return QImage::Format_Invalid;
            }

        case QOpenGLTexture::Int16:
        case QOpenGLTexture::UInt16:
            switch (format) {
            case QOpenGLTexture::RGBA:
            case QOpenGLTexture::RGBA_Integer: return QImage::Format_RGBA64;

            default: return QImage::Format_Invalid;
            }

        case QOpenGLTexture::UInt32_D24S8: return QImage::Format_RGBA8888;

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

    bool isDepthStencilFormat(QOpenGLTexture::TextureFormat format)
    {
        switch (format) {
        case QOpenGLTexture::D16:
        case QOpenGLTexture::D24:
        case QOpenGLTexture::D24S8:
        case QOpenGLTexture::D32:
        case QOpenGLTexture::D32F:
        case QOpenGLTexture::D32FS8X24:
        case QOpenGLTexture::S8:        return true;
        default:                        return false;
        }
    }

    bool canGenerateMipmaps(QOpenGLTexture::Target target,
        QOpenGLTexture::TextureFormat format)
    {
        switch (target) {
        case QOpenGLTexture::Target1D:
        case QOpenGLTexture::Target1DArray:
        case QOpenGLTexture::Target2D:
        case QOpenGLTexture::Target2DArray:
        case QOpenGLTexture::Target3D:
        case QOpenGLTexture::TargetCubeMap:
        case QOpenGLTexture::TargetCubeMapArray: break;
        default:                                 return false;
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

    bool convertPlane(const uchar *source,
        QOpenGLTexture::TextureFormat sourceFormat, uchar *dest,
        QOpenGLTexture::TextureFormat destFormat, int pixels)
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
} // namespace

bool isMultisampleTarget(QOpenGLTexture::Target target)
{
    return (target == QOpenGLTexture::Target2DMultisample
        || target == QOpenGLTexture::Target2DMultisampleArray);
}

bool isCubemapTarget(QOpenGLTexture::Target target)
{
    return (target == QOpenGLTexture::TargetCubeMap
        || target == QOpenGLTexture::TargetCubeMapArray);
}

TextureSampleType getTextureSampleType(QOpenGLTexture::TextureFormat format)
{
    switch (format) {
    case QOpenGLTexture::SRGB8:
    case QOpenGLTexture::SRGB8_Alpha8:
        return TextureSampleType::Normalized_sRGB;

    case QOpenGLTexture::R16F:
    case QOpenGLTexture::RG16F:
    case QOpenGLTexture::RGB16F:
    case QOpenGLTexture::RGBA16F:
    case QOpenGLTexture::R32F:
    case QOpenGLTexture::RG32F:
    case QOpenGLTexture::RGB32F:
    case QOpenGLTexture::RGBA32F:
    case QOpenGLTexture::RGB9E5:
    case QOpenGLTexture::RG11B10F: return TextureSampleType::Float;

    case QOpenGLTexture::R8U:
    case QOpenGLTexture::RG8U:
    case QOpenGLTexture::RGB8U:
    case QOpenGLTexture::RGBA8U:
    case QOpenGLTexture::S8:     return TextureSampleType::Uint8;

    case QOpenGLTexture::R16U:
    case QOpenGLTexture::RG16U:
    case QOpenGLTexture::RGB16U:
    case QOpenGLTexture::RGBA16U: return TextureSampleType::Uint16;

    case QOpenGLTexture::R32U:
    case QOpenGLTexture::RG32U:
    case QOpenGLTexture::RGB32U:
    case QOpenGLTexture::RGBA32U: return TextureSampleType::Uint32;

    case QOpenGLTexture::R8I:
    case QOpenGLTexture::RG8I:
    case QOpenGLTexture::RGB8I:
    case QOpenGLTexture::RGBA8I: return TextureSampleType::Int8;

    case QOpenGLTexture::R16I:
    case QOpenGLTexture::RG16I:
    case QOpenGLTexture::RGB16I:
    case QOpenGLTexture::RGBA16I: return TextureSampleType::Int16;

    case QOpenGLTexture::R32I:
    case QOpenGLTexture::RG32I:
    case QOpenGLTexture::RGB32I:
    case QOpenGLTexture::RGBA32I: return TextureSampleType::Int32;

    case QOpenGLTexture::RGB10A2: return TextureSampleType::Uint_10_10_10_2;

    default: return TextureSampleType::Normalized;
    }
}

TextureDataType getTextureDataType(QOpenGLTexture::TextureFormat format)
{
    switch (format) {
    case QOpenGLTexture::R8_UNorm:
    case QOpenGLTexture::RG8_UNorm:
    case QOpenGLTexture::RGB8_UNorm:
    case QOpenGLTexture::RGBA8_UNorm:
    case QOpenGLTexture::SRGB8:
    case QOpenGLTexture::SRGB8_Alpha8:
    case QOpenGLTexture::R8U:
    case QOpenGLTexture::RG8U:
    case QOpenGLTexture::RGB8U:
    case QOpenGLTexture::RGBA8U:
    case QOpenGLTexture::S8:           return TextureDataType::Uint8;

    case QOpenGLTexture::R16_UNorm:
    case QOpenGLTexture::RG16_UNorm:
    case QOpenGLTexture::RGB16_UNorm:
    case QOpenGLTexture::RGBA16_UNorm:
    case QOpenGLTexture::R16U:
    case QOpenGLTexture::RG16U:
    case QOpenGLTexture::RGB16U:
    case QOpenGLTexture::RGBA16U:
    case QOpenGLTexture::D16:          return TextureDataType::Uint16;

    case QOpenGLTexture::R8_SNorm:
    case QOpenGLTexture::RG8_SNorm:
    case QOpenGLTexture::RGB8_SNorm:
    case QOpenGLTexture::RGBA8_SNorm:
    case QOpenGLTexture::R8I:
    case QOpenGLTexture::RG8I:
    case QOpenGLTexture::RGB8I:
    case QOpenGLTexture::RGBA8I:      return TextureDataType::Int8;

    case QOpenGLTexture::R16_SNorm:
    case QOpenGLTexture::RG16_SNorm:
    case QOpenGLTexture::RGB16_SNorm:
    case QOpenGLTexture::RGBA16_SNorm:
    case QOpenGLTexture::R16I:
    case QOpenGLTexture::RG16I:
    case QOpenGLTexture::RGB16I:
    case QOpenGLTexture::RGBA16I:      return TextureDataType::Int16;

    case QOpenGLTexture::R32U:
    case QOpenGLTexture::RG32U:
    case QOpenGLTexture::RGB32U:
    case QOpenGLTexture::RGBA32U:
    case QOpenGLTexture::D32:     return TextureDataType::Uint32;

    case QOpenGLTexture::R32I:
    case QOpenGLTexture::RG32I:
    case QOpenGLTexture::RGB32I:
    case QOpenGLTexture::RGBA32I: return TextureDataType::Int32;

    case QOpenGLTexture::R16F:
    case QOpenGLTexture::RG16F:
    case QOpenGLTexture::RGB16F:
    case QOpenGLTexture::RGBA16F: return TextureDataType::Float16;

    case QOpenGLTexture::R32F:
    case QOpenGLTexture::RG32F:
    case QOpenGLTexture::RGB32F:
    case QOpenGLTexture::RGBA32F:
    case QOpenGLTexture::D32F:    return TextureDataType::Float32;

    default: return TextureDataType::Other;
    }
}

int getTextureDataSize(QOpenGLTexture::TextureFormat format)
{
    switch (getTextureDataType(format)) {
    case TextureDataType::Int8:    return 1;
    case TextureDataType::Int16:   return 2;
    case TextureDataType::Int32:   return 4;
    case TextureDataType::Uint8:   return 1;
    case TextureDataType::Uint16:  return 2;
    case TextureDataType::Uint32:  return 4;
    case TextureDataType::Float16: return 2;
    case TextureDataType::Float32: return 4;
    case TextureDataType::Other:   return 0;
    }
    return 0;
}

int getTextureComponentCount(QOpenGLTexture::TextureFormat format)
{
    switch (format) {
    case QOpenGLTexture::R8_UNorm:
    case QOpenGLTexture::R8_SNorm:
    case QOpenGLTexture::R16_UNorm:
    case QOpenGLTexture::R16_SNorm:
    case QOpenGLTexture::R8U:
    case QOpenGLTexture::R8I:
    case QOpenGLTexture::R16U:
    case QOpenGLTexture::R16I:
    case QOpenGLTexture::R32U:
    case QOpenGLTexture::R32I:
    case QOpenGLTexture::R16F:
    case QOpenGLTexture::R32F:
    case QOpenGLTexture::D16:
    case QOpenGLTexture::D24:
    case QOpenGLTexture::D32:
    case QOpenGLTexture::D32F:
    case QOpenGLTexture::D24S8:
    case QOpenGLTexture::D32FS8X24:
    case QOpenGLTexture::S8:
    case QOpenGLTexture::R_ATI1N_UNorm:
    case QOpenGLTexture::R_ATI1N_SNorm:
    case QOpenGLTexture::R11_EAC_UNorm:
    case QOpenGLTexture::R11_EAC_SNorm: return 1;

    case QOpenGLTexture::RG8_UNorm:
    case QOpenGLTexture::RG8_SNorm:
    case QOpenGLTexture::RG16_UNorm:
    case QOpenGLTexture::RG16_SNorm:
    case QOpenGLTexture::RG8U:
    case QOpenGLTexture::RG8I:
    case QOpenGLTexture::RG16U:
    case QOpenGLTexture::RG16I:
    case QOpenGLTexture::RG32U:
    case QOpenGLTexture::RG32I:
    case QOpenGLTexture::RG16F:
    case QOpenGLTexture::RG32F:
    case QOpenGLTexture::RG_ATI2N_UNorm:
    case QOpenGLTexture::RG_ATI2N_SNorm:
    case QOpenGLTexture::RGB_BP_UNSIGNED_FLOAT:
    case QOpenGLTexture::RGB_BP_SIGNED_FLOAT:
    case QOpenGLTexture::RG11_EAC_UNorm:
    case QOpenGLTexture::RG11_EAC_SNorm:        return 2;

    case QOpenGLTexture::RGB8_UNorm:
    case QOpenGLTexture::RGB8_SNorm:
    case QOpenGLTexture::RGB16_UNorm:
    case QOpenGLTexture::RGB16_SNorm:
    case QOpenGLTexture::RGB8U:
    case QOpenGLTexture::RGB8I:
    case QOpenGLTexture::RGB16U:
    case QOpenGLTexture::RGB16I:
    case QOpenGLTexture::RGB32U:
    case QOpenGLTexture::RGB32I:
    case QOpenGLTexture::RGB16F:
    case QOpenGLTexture::RGB32F:
    case QOpenGLTexture::SRGB8:
    case QOpenGLTexture::SRGB_DXT1:
    case QOpenGLTexture::RGB9E5:
    case QOpenGLTexture::RG11B10F:
    case QOpenGLTexture::RG3B2:
    case QOpenGLTexture::R5G6B5:
    case QOpenGLTexture::RGB_DXT1:
    case QOpenGLTexture::RGB8_ETC2:
    case QOpenGLTexture::SRGB8_ETC2:  return 3;

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

bool TextureData::create(QOpenGLTexture::Target target,
    QOpenGLTexture::TextureFormat format, int width, int height, int depth,
    int layers, int levels)
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
    case QOpenGLTexture::Target1DArray:
        createInfo.isArray = KTX_TRUE;
        createInfo.numLayers = static_cast<ktx_uint32_t>(layers);
        [[fallthrough]];
    case QOpenGLTexture::Target1D: createInfo.numDimensions = 1; break;

    case QOpenGLTexture::Target2DArray:
        createInfo.isArray = KTX_TRUE;
        createInfo.numLayers = static_cast<ktx_uint32_t>(layers);
        [[fallthrough]];
    case QOpenGLTexture::Target2D:
    case QOpenGLTexture::TargetRectangle:
        createInfo.numDimensions = 2;
        createInfo.baseHeight = static_cast<ktx_uint32_t>(height);
        break;

    case QOpenGLTexture::Target3D:
        createInfo.numDimensions = 3;
        createInfo.baseHeight = static_cast<ktx_uint32_t>(height);
        createInfo.baseDepth = static_cast<ktx_uint32_t>(depth);
        break;

    case QOpenGLTexture::TargetCubeMapArray:
        createInfo.isArray = KTX_TRUE;
        createInfo.numLayers = static_cast<ktx_uint32_t>(layers);
        createInfo.numLayers *= 6;
        [[fallthrough]];
    case QOpenGLTexture::TargetCubeMap:
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

TextureData TextureData::convert(QOpenGLTexture::TextureFormat format)
{
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

    return copy;
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
#if !defined(OpenImageIO_FOUND)
    return false;
#else // OpenImageIO_FOUND
    using namespace OIIO;
    auto input = ImageInput::open(fileName.toStdWString());
    if (!input)
        return false;

    using F = QOpenGLTexture::TextureFormat;
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
    const auto target =
        (spec.depth > 1 ? QOpenGLTexture::Target3D : QOpenGLTexture::Target2D);

    if (!create(target, format, spec.width, spec.height, spec.depth, 1))
        return false;

    // TODO: load all layers/levels
    if (!input->read_image(0, 0, 0, -1, spec.format, getWriteonlyData(0, 0, 0)))
        return false;

    return true;
#endif // OpenImageIO_FOUND
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
        image = std::move(image).mirrored();

    if (!create(QOpenGLTexture::Target2D, getTextureFormat(image.format()),
            image.width(), image.height(), 1, 1))
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
    std::fscanf(f, "%c%c\n", &c0, &c1);
    if (c0 != 'P')
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
    if (!fscanf(f, "%d %d\n", &width, &height))
        return false;
    std::fscanf(f, "%lf\n", &scale);
    auto endianness = QSysInfo::BigEndian;
    if (scale < 0) {
        endianness = QSysInfo::LittleEndian;
        scale = -scale;
    }
    std::fscanf(f, "\n");

    const auto format = (channels == 3 ? QOpenGLTexture::TextureFormat::RGB32F
                                       : QOpenGLTexture::TextureFormat::R32F);
    if (!create(QOpenGLTexture::Target2D, format, width, height, 1, 1))
        return false;
    const auto size = getImageSize(0);
    const auto data = getWriteonlyData(0, 0, 0);

    if (endianness == QSysInfo::ByteOrder) {
        std::fread(data, size, 1, f);
    } else {
        auto buffer = QByteArray(getImageSize(0), 0);
        std::fread(buffer.data(), size, 1, f);
        if (endianness == QSysInfo::LittleEndian) {
            qFromLittleEndian<float>(buffer.data(), size / 4, data);
        } else {
            qFromBigEndian<float>(buffer.data(), size / 4, data);
        }
    }
    return true;
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
#if !defined(OpenImageIO_FOUND)
    return false;
#else // OpenImageIO_FOUND
    using namespace OIIO;

    const auto typeDesc = [&]() {
        switch (getTextureDataType(format())) {
        case TextureDataType::Uint8:   return TypeDesc::UINT8;
        case TextureDataType::Int8:    return TypeDesc::INT8;
        case TextureDataType::Uint16:  return TypeDesc::UINT16;
        case TextureDataType::Int16:   return TypeDesc::INT16;
        case TextureDataType::Uint32:  return TypeDesc::UINT32;
        case TextureDataType::Int32:   return TypeDesc::INT32;
        case TextureDataType::Float16: return TypeDesc::HALF;
        case TextureDataType::Float32: return TypeDesc::FLOAT;
        case TextureDataType::Other:   return TypeDesc::NONE;
        }
        return TypeDesc::NONE;
    }();
    if (typeDesc == TypeDesc::NONE)
        return false;

    auto output = ImageOutput::create(fileName.toStdWString());
    if (!output)
        return false;

    const auto channelCount = getTextureComponentCount(format());
    const auto spec = ImageSpec(width(), height(), channelCount, typeDesc);
    if (!output->open(fileName.toStdWString(), spec))
        return false;
    if (!output->write_image(typeDesc, getData(0, 0, 0)))
        return false;
    return true;
#endif // OpenImageIO_FOUND
}

bool TextureData::saveQImage(const QString &fileName, bool flipVertically) const
{
    auto image = toImage();
    if (image.isNull())
        return false;

    if (flipVertically)
        image = std::move(image).mirrored();

    return image.save(fileName);
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

QOpenGLTexture::Target TextureData::getTarget(int samples) const
{
    if (!mKtxTexture)
        return {};
    const auto &texture = *mKtxTexture;
    if (texture.isCubemap)
        return (texture.isArray ? QOpenGLTexture::TargetCubeMapArray
                                : QOpenGLTexture::TargetCubeMap);
    switch (texture.numDimensions) {
    case 1:
        return (texture.isArray ? QOpenGLTexture::Target1DArray
                                : QOpenGLTexture::Target1D);
    case 2:
        if (samples > 1)
            return (texture.isArray ? QOpenGLTexture::Target2DMultisampleArray
                                    : QOpenGLTexture::Target2DMultisample);
        return (texture.isArray ? QOpenGLTexture::Target2DArray
                                : QOpenGLTexture::Target2D);
    case 3: return QOpenGLTexture::Target3D;
    }
    return {};
}

QOpenGLTexture::TextureFormat TextureData::format() const
{
    return (isNull() ? QOpenGLTexture::TextureFormat::NoFormat
                     : static_cast<QOpenGLTexture::TextureFormat>(
                           mKtxTexture->glInternalformat));
}

QOpenGLTexture::PixelFormat TextureData::pixelFormat() const
{
    return (isNull()
            ? QOpenGLTexture::PixelFormat::NoSourceFormat
            : static_cast<QOpenGLTexture::PixelFormat>(mKtxTexture->glFormat));
}

QOpenGLTexture::PixelType TextureData::pixelType() const
{
    return (isNull()
            ? QOpenGLTexture::PixelType::NoPixelType
            : static_cast<QOpenGLTexture::PixelType>(mKtxTexture->glType));
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

int TextureData::levels() const
{
    return (isNull() ? 0 : static_cast<int>(mKtxTexture->numLevels));
}

int TextureData::layers() const
{
    if (getTarget() == QOpenGLTexture::TargetCubeMapArray)
        return static_cast<int>(mKtxTexture->numLayers / 6);

    return (isNull() ? 0 : static_cast<int>(mKtxTexture->numLayers));
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

int TextureData::getImageSize(int level) const
{
    if (isNull())
        return 0;
    return (static_cast<int>(
               ktxTexture_GetImageSize(ktxTexture(mKtxTexture.get()),
                   static_cast<ktx_uint32_t>(level))))
        * std::max(depth() >> level, 1);
}

int TextureData::getLevelSize(int level) const
{
    return getImageSize(level) * layers() * faces();
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
    const auto pitch = getLevelSize(0) / height();
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

bool TextureData::uploadVK(ktxVulkanDeviceInfo *vdi,
    ktxVulkanTexture *vkTexture, VkImageUsageFlags usageFlags,
    VkImageLayout initialLayout) const
{
    if (isNull() || !vkTexture || !vdi)
        return false;

    const auto result = ktxTexture_VkUploadEx(ktxTexture(mKtxTexture.get()),
        vdi, vkTexture, VK_IMAGE_TILING_OPTIMAL, usageFlags, initialLayout);

    return (result == KTX_SUCCESS);
}
