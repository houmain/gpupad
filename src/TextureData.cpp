#include "TextureData.h"
#include "session/Item.h"
#include <cstring>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QScopeGuard>
#include <QImageReader>
#include <QtEndian>

#if defined(OPENIMAGEIO_ENABLED)
#  pragma warning(disable: 4267)
#  include <OpenImageIO/imageio.h>
#endif

namespace {
    QImage::Format getNextNativeImageFormat(QImage::Format format)
    {
        switch (format) {
            case QImage::Format_Mono:
            case QImage::Format_MonoLSB:
            case QImage::Format_Alpha8:
            case QImage::Format_Grayscale8:
                return QImage::Format_Grayscale8;
            case QImage::Format_BGR30:
            case QImage::Format_A2BGR30_Premultiplied:
            case QImage::Format_RGB30:
            case QImage::Format_A2RGB30_Premultiplied:
            case QImage::Format_RGBX64:
            case QImage::Format_RGBA64:
            case QImage::Format_RGBA64_Premultiplied:
                return QImage::Format_RGBA64;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 13, 0))
            case QImage::Format_Grayscale16:
                return QImage::Format_Grayscale16;
#endif
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
#if (QT_VERSION >= QT_VERSION_CHECK(5, 13, 0))
            case QImage::Format_Grayscale16:
                return QOpenGLTexture::R16_UNorm;
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(6, 2, 0))
            case QImage::Format_RGBA16FPx4:
                return QOpenGLTexture::RGBA16F;
            case QImage::Format_RGBA32FPx4:
                return QOpenGLTexture::RGBA32F;
#endif
            default:
                return QOpenGLTexture::NoFormat;
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

    QOpenGLTexture::Target getTarget(const ktxTexture1 &texture)
    {
        if (texture.isCubemap)
            return (texture.isArray ?
                QOpenGLTexture::TargetCubeMapArray : QOpenGLTexture::TargetCubeMap);
        switch (texture.numDimensions) {
            case 1: return (texture.isArray ?
                QOpenGLTexture::Target1DArray : QOpenGLTexture::Target1D);
            case 2: return (texture.isArray ?
                QOpenGLTexture::Target2DArray : QOpenGLTexture::Target2D);
            case 3: return QOpenGLTexture::Target3D;
        }
        return { };
    }

    bool isMultisampleTarget(QOpenGLTexture::Target target)
    {
        return (target == QOpenGLTexture::Target2DMultisample ||
                target == QOpenGLTexture::Target2DMultisampleArray);
    }

    bool isCubemapTarget(QOpenGLTexture::Target target)
    {
        return (target == QOpenGLTexture::TargetCubeMap ||
                target == QOpenGLTexture::TargetCubeMapArray);
    }

    ktx_uint32_t getLevelCount(const ktxTextureCreateInfo &info)
    {
        auto dimension = std::max(std::max(
            info.baseWidth, info.baseHeight),
            info.baseDepth);
        auto levels = ktx_uint32_t{ };
        for (; dimension; dimension >>= 1)
            ++levels;
        return levels;
    }

    bool canGenerateMipmaps(const QOpenGLTexture::Target target,
        const QOpenGLTexture::TextureFormat format)
    {
        switch (target) {
            case QOpenGLTexture::Target1D:
            case QOpenGLTexture::Target1DArray:
            case QOpenGLTexture::Target2D:
            case QOpenGLTexture::Target2DArray:
            case QOpenGLTexture::Target3D:
            case QOpenGLTexture::TargetCubeMap:
            case QOpenGLTexture::TargetCubeMapArray:
                break;
            default:
                return false;
        }
        const auto sampleType = getTextureSampleType(format);
        return (sampleType == TextureSampleType::Normalized ||
                sampleType == TextureSampleType::Normalized_sRGB ||
                sampleType == TextureSampleType::Float);
    }

    GLuint createFramebuffer(QOpenGLFunctions_3_3_Core& gl, GLenum target, GLuint textureId, GLenum attachment)
    {
        auto fbo = GLuint{ };
        gl.glGenFramebuffers(1, &fbo);
        gl.glBindFramebuffer(target, fbo);
        gl.glFramebufferTexture(target, attachment, textureId, 0);
        return fbo;
    }

    bool resolveTexture(QOpenGLFunctions_3_3_Core& gl, GLuint sourceTextureId,
        GLuint destTextureId, int width, int height, QOpenGLTexture::TextureFormat format)
    {
        auto blitMask = GLbitfield{ };
        auto attachment = GLenum{ };
        switch (format) {
            default:
                blitMask = GL_COLOR_BUFFER_BIT;
                attachment = GL_COLOR_ATTACHMENT0;
                break;

            case QOpenGLTexture::D16:
            case QOpenGLTexture::D24:
            case QOpenGLTexture::D32:
            case QOpenGLTexture::D32F:
                blitMask = GL_DEPTH_BUFFER_BIT;
                attachment = GL_DEPTH_ATTACHMENT;
                break;

            case QOpenGLTexture::S8:
                blitMask = GL_STENCIL_BUFFER_BIT;
                attachment = GL_STENCIL_ATTACHMENT;
                break;

            case QOpenGLTexture::D24S8:
            case QOpenGLTexture::D32FS8X24:
                blitMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
                attachment = GL_DEPTH_STENCIL_ATTACHMENT;
                break;
        }

        auto previousTarget = GLint{ };
        gl.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousTarget);
        const auto sourceFbo = createFramebuffer(gl, GL_READ_FRAMEBUFFER, sourceTextureId, attachment);
        const auto destFbo = createFramebuffer(gl, GL_DRAW_FRAMEBUFFER, destTextureId, attachment);
        gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, blitMask, GL_NEAREST);
        gl.glDeleteFramebuffers(1, &sourceFbo);
        gl.glDeleteFramebuffers(1, &destFbo);
        gl.glBindFramebuffer(GL_FRAMEBUFFER, previousTarget);
        return (glGetError() == GL_NONE);
    }
} // namespace

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
        case QOpenGLTexture::RG11B10F:
            return TextureSampleType::Float;

        case QOpenGLTexture::R8U:
        case QOpenGLTexture::RG8U:
        case QOpenGLTexture::RGB8U:
        case QOpenGLTexture::RGBA8U:
        case QOpenGLTexture::S8:
            return TextureSampleType::Uint8;

        case QOpenGLTexture::R16U:
        case QOpenGLTexture::RG16U:
        case QOpenGLTexture::RGB16U:
        case QOpenGLTexture::RGBA16U:
            return TextureSampleType::Uint16;

        case QOpenGLTexture::R32U:
        case QOpenGLTexture::RG32U:
        case QOpenGLTexture::RGB32U:
        case QOpenGLTexture::RGBA32U:
            return TextureSampleType::Uint32;

        case QOpenGLTexture::R8I:
        case QOpenGLTexture::RG8I:
        case QOpenGLTexture::RGB8I:
        case QOpenGLTexture::RGBA8I:
            return TextureSampleType::Int8;

        case QOpenGLTexture::R16I:
        case QOpenGLTexture::RG16I:
        case QOpenGLTexture::RGB16I:
        case QOpenGLTexture::RGBA16I:
            return TextureSampleType::Int16;

        case QOpenGLTexture::R32I:
        case QOpenGLTexture::RG32I:
        case QOpenGLTexture::RGB32I:
        case QOpenGLTexture::RGBA32I:
            return TextureSampleType::Int32;

        case QOpenGLTexture::RGB10A2:
            return TextureSampleType::Uint_10_10_10_2;

        default:
            return TextureSampleType::Normalized;
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
        case QOpenGLTexture::S8:
            return TextureDataType::Uint8;

        case QOpenGLTexture::R16_UNorm:
        case QOpenGLTexture::RG16_UNorm:
        case QOpenGLTexture::RGB16_UNorm:
        case QOpenGLTexture::RGBA16_UNorm:
        case QOpenGLTexture::R16U:
        case QOpenGLTexture::RG16U:
        case QOpenGLTexture::RGB16U:
        case QOpenGLTexture::RGBA16U:
        case QOpenGLTexture::D16:
            return TextureDataType::Uint16;

        case QOpenGLTexture::R8_SNorm:
        case QOpenGLTexture::RG8_SNorm:
        case QOpenGLTexture::RGB8_SNorm:
        case QOpenGLTexture::RGBA8_SNorm:
        case QOpenGLTexture::R8I:
        case QOpenGLTexture::RG8I:
        case QOpenGLTexture::RGB8I:
        case QOpenGLTexture::RGBA8I:
            return TextureDataType::Int8;

        case QOpenGLTexture::R16_SNorm:
        case QOpenGLTexture::RG16_SNorm:
        case QOpenGLTexture::RGB16_SNorm:
        case QOpenGLTexture::RGBA16_SNorm:
        case QOpenGLTexture::R16I:
        case QOpenGLTexture::RG16I:
        case QOpenGLTexture::RGB16I:
        case QOpenGLTexture::RGBA16I:
            return TextureDataType::Int16;

        case QOpenGLTexture::R32U:
        case QOpenGLTexture::RG32U:
        case QOpenGLTexture::RGB32U:
        case QOpenGLTexture::RGBA32U:
        case QOpenGLTexture::D32:
            return TextureDataType::Uint32;

        case QOpenGLTexture::R32I:
        case QOpenGLTexture::RG32I:
        case QOpenGLTexture::RGB32I:
        case QOpenGLTexture::RGBA32I:
            return TextureDataType::Int32;

        case QOpenGLTexture::R16F:
        case QOpenGLTexture::RG16F:
        case QOpenGLTexture::RGB16F:
        case QOpenGLTexture::RGBA16F:
            return TextureDataType::Float16;

        case QOpenGLTexture::R32F:
        case QOpenGLTexture::RG32F:
        case QOpenGLTexture::RGB32F:
        case QOpenGLTexture::RGBA32F:
        case QOpenGLTexture::D32F:
            return TextureDataType::Float32;

        default:
            return TextureDataType::Other;
    }
}

int getTextureDataSize(QOpenGLTexture::TextureFormat format)
{
    switch (getTextureDataType(format)) {
        case TextureDataType::Int8: return 1;
        case TextureDataType::Int16: return 2;
        case TextureDataType::Int32: return 4;
        case TextureDataType::Uint8: return 1;
        case TextureDataType::Uint16: return 2;
        case TextureDataType::Uint32: return 4;
        case TextureDataType::Float16: return 2;
        case TextureDataType::Float32: return 4;
        case TextureDataType::Other: return 0;
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
        case QOpenGLTexture::R11_EAC_SNorm:
            return 1;

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
        case QOpenGLTexture::RG11_EAC_SNorm:
            return 2;

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
        case QOpenGLTexture::SRGB8_ETC2:
            return 3;

        default:
            return 4;
    }
}

bool operator==(const TextureData &a, const TextureData &b) 
{
    if (a.target() != b.target() ||
        a.format() != b.format() ||
        a.width() != b.width() ||
        a.height() != b.height() ||
        a.depth() != b.depth() ||
        a.levels() != b.levels() ||
        a.layers() != b.layers() ||
        a.faces() != b.faces() ||
        a.samples() != b.samples())
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
                if (sa != sb ||
                    std::memcmp(da, db, static_cast<size_t>(sa)) != 0)
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

bool TextureData::create(
    QOpenGLTexture::Target target,
    QOpenGLTexture::TextureFormat format,
    int width, int height, int depth, int layers, 
    int samples, int levels)
{
    if (width <= 0 || height <= 0 || depth <= 0 ||
        layers <= 0 || samples <= 0 || levels < 0)
      return false;

    auto createInfo = ktxTextureCreateInfo{ };
    createInfo.glInternalformat = format;
    createInfo.baseWidth = static_cast<ktx_uint32_t>(width);
    createInfo.baseHeight = 1;
    createInfo.baseDepth = 1;
    createInfo.numLayers = 1;
    createInfo.numFaces = 1;
    createInfo.isArray = KTX_FALSE;

    switch (target) {
        case QOpenGLTexture::Target1DArray:
            createInfo.isArray = KTX_TRUE;
            createInfo.numLayers = static_cast<ktx_uint32_t>(layers);
            [[fallthrough]];
        case QOpenGLTexture::Target1D:
            createInfo.numDimensions = 1;
            break;

        case QOpenGLTexture::Target2DMultisampleArray:
        case QOpenGLTexture::Target2DArray:
            createInfo.isArray = KTX_TRUE;
            createInfo.numLayers = static_cast<ktx_uint32_t>(layers);
            [[fallthrough]];
        case QOpenGLTexture::Target2D:
        case QOpenGLTexture::Target2DMultisample:
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

        default:
            return false;
    }

    createInfo.numLevels = (levels > 0 ? levels :
      (!canGenerateMipmaps(target, format) ? 1 :
        getLevelCount(createInfo)));

      auto texture = std::add_pointer_t<ktxTexture1>{ };
    if (ktxTexture1_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE,
            &texture) == KTX_SUCCESS) {
        mKtxTexture.reset(texture, [](ktxTexture1* tex) { ktxTexture_Destroy(ktxTexture(tex)); });
        mTarget = target;
        mSamples = (isMultisampleTarget(target) ? samples : 1);
        return true;
    }
    return false;
}

bool TextureData::loadKtx(const QString &fileName, bool flipVertically)
{
    auto texture = std::add_pointer_t<ktxTexture1>{ };
    if (ktxTexture1_CreateFromNamedFile(qUtf8Printable(fileName),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) != KTX_SUCCESS)
        return false;

    mTarget = getTarget(*texture);
    mKtxTexture.reset(texture, [](ktxTexture1* tex) { ktxTexture_Destroy(ktxTexture(tex)); });
    
    if (flipVertically)
        this->flipVertically();

    return true;
}

bool TextureData::loadOpenImageIO(const QString &fileName, bool flipVertically)
{
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
            case TypeDesc::UINT8: return channel(F::R8_UNorm, F::RG8_UNorm, F::RGB8_UNorm, F::RGBA8_UNorm);
            case TypeDesc::INT8: return channel(F::R8_SNorm, F::RG8_SNorm, F::RGB8_SNorm, F::RGBA8_SNorm);
            case TypeDesc::UINT16: return channel(F::R16_UNorm, F::RG16_UNorm, F::RGB16_UNorm, F::RGBA16_UNorm);
            case TypeDesc::INT16: return channel(F::R16_SNorm, F::RG16_SNorm, F::RGB16_SNorm, F::RGBA16_SNorm);
            case TypeDesc::UINT32: return channel(F::R32U, F::RG32U, F::RGB32U, F::RGBA32U);
            case TypeDesc::INT32: return channel(F::R32I, F::RG32I, F::RGB32I, F::RGBA32I);
            //case TypeDesc::UINT64: return channel(F::R64U, F::RG64U, F::RGB64U, F::RGBA64U);
            //case TypeDesc::INT64: return channel(F::R64I, F::RG64I, F::RGB64I, F::RGBA64I);
            case TypeDesc::HALF: return channel(F::R16F, F::RG16F, F::RGB16F, F::RGBA16F);
            case TypeDesc::FLOAT: return channel(F::R32F, F::RG32F, F::RGB32F, F::RGBA32F);
            //case TypeDesc::TOUBLE: return channel(F::R64F, F::RG64F, F::RGB64F, F::RGBA64F);
        }
        return F::NoFormat;
    }();
    const auto target = (spec.depth > 1 ?
        QOpenGLTexture::Target3D : QOpenGLTexture::Target2D);

    if (!create(target, format, spec.width, spec.height, spec.depth))
        return false;

    // TODO: load all layers/levels
    if (!input->read_image(0, 0, 0, -1, spec.format, getWriteonlyData(0, 0, 0)))
        return false;

    return true;
}

bool TextureData::loadQImage(const QString &fileName, bool flipVertically)
{
    auto imageReader = QImageReader(fileName);
    imageReader.setAutoTransform(true);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    imageReader.setAllocationLimit(0);
#endif

    auto image = QImage();
    if (!imageReader.read(&image))
        return false;

    return loadQImage(std::move(image), flipVertically);
}

bool TextureData::loadQImage(QImage image, bool flipVertically) 
{
    image = std::move(image).convertToFormat(getNextNativeImageFormat(image.format()));

    if (flipVertically)
        image = std::move(image).mirrored();

    if (!create(QOpenGLTexture::Target2D, getTextureFormat(image.format()),
                image.width(), image.height()))
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

    auto c0 = char{ };
    auto c1 = char{ };
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

    const auto format = (channels == 3 ?
        QOpenGLTexture::TextureFormat::RGB32F :
        QOpenGLTexture::TextureFormat::R32F);
    if (!create(QOpenGLTexture::Target2D, format, width, height))
        return false;
    const auto size = getImageSize(0);
    const auto data = getWriteonlyData(0, 0, 0);

    if (endianness == QSysInfo::ByteOrder) {
        std::fread(data, size, 1, f);
    }
    else {
        auto buffer = QByteArray(getImageSize(0), 0);
        std::fread(buffer.data(), size, 1, f);
        if (endianness == QSysInfo::LittleEndian)
            qFromLittleEndian<float>(buffer.data(), size / 4, data);
        else
            qFromBigEndian<float>(buffer.data(), size / 4, data);
    }
    return true;
}

bool TextureData::load(const QString &fileName, bool flipVertically)
{
    return loadKtx(fileName, flipVertically) ||
           loadPfm(fileName, flipVertically) ||
           loadOpenImageIO(fileName, flipVertically) ||
           loadQImage(fileName, flipVertically);
}

bool TextureData::saveKtx(const QString &fileName, bool flipVertically) const
{
    if (!fileName.endsWith(".ktx", Qt::CaseInsensitive))
        return false;

    return (ktxTexture_WriteToNamedFile(ktxTexture(mKtxTexture.get()), 
        qUtf8Printable(fileName)) == KTX_SUCCESS);
}

bool TextureData::savePfm(const QString &fileName, bool flipVertically) const
{
    if (!fileName.endsWith(".pfm", Qt::CaseInsensitive))
        return false;

    // TODO
    return false;
}

bool TextureData::saveOpenImageIO(const QString &fileName, bool flipVertically) const
{
    using namespace OIIO;

    const auto typeDesc = [&]() {
        switch (getTextureDataType(format())) {
            case TextureDataType::Uint8: return TypeDesc::UINT8;
            case TextureDataType::Int8: return TypeDesc::INT8;
            case TextureDataType::Uint16: return TypeDesc::UINT16;
            case TextureDataType::Int16: return TypeDesc::INT16;
            case TextureDataType::Uint32: return TypeDesc::UINT32;
            case TextureDataType::Int32: return TypeDesc::INT32;
            case TextureDataType::Float16: return TypeDesc::HALF;
            case TextureDataType::Float32: return TypeDesc::FLOAT;
            case TextureDataType::Other: return TypeDesc::NONE;
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
    return saveKtx(fileName, flipVertically) ||
           savePfm(fileName, flipVertically) ||
           saveOpenImageIO(fileName, flipVertically) ||
           saveQImage(fileName, flipVertically);
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
        return { };

    const auto imageFormat = getImageFormat(pixelFormat(), pixelType());
    if (imageFormat == QImage::Format_Invalid)
        return { };

    auto image = QImage(width(), height(), imageFormat);
    if (static_cast<int>(image.sizeInBytes()) != getImageSize(0))
        return { };

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

bool TextureData::isMultisample() const
{
    return isMultisampleTarget(mTarget);
}

int TextureData::dimensions() const
{
    return (isNull() ? 0 :
        static_cast<int>(mKtxTexture->numDimensions));
}

QOpenGLTexture::TextureFormat TextureData::format() const
{
    return (isNull() ? QOpenGLTexture::TextureFormat::NoFormat : 
        static_cast<QOpenGLTexture::TextureFormat>(mKtxTexture->glInternalformat));
}

QOpenGLTexture::PixelFormat TextureData::pixelFormat() const
{
    return (isNull() ? QOpenGLTexture::PixelFormat::NoSourceFormat :
        static_cast<QOpenGLTexture::PixelFormat>(mKtxTexture->glFormat));
}

QOpenGLTexture::PixelType TextureData::pixelType() const
{
    return (isNull() ? QOpenGLTexture::PixelType::NoPixelType :
        static_cast<QOpenGLTexture::PixelType>(mKtxTexture->glType));
}

int TextureData::getLevelWidth(int level) const
{
    return (isNull() ? 0 :
        std::max(static_cast<int>(mKtxTexture->baseWidth) >> level, 1));
}

int TextureData::getLevelHeight(int level) const
{
    return (isNull() ? 0 :
        std::max(static_cast<int>(mKtxTexture->baseHeight) >> level, 1));
}

int TextureData::getLevelDepth(int level) const
{
    return (isNull() ? 0 :
        std::max(static_cast<int>(mKtxTexture->baseDepth) >> level, 1));
}

int TextureData::levels() const
{
    return (isNull() ? 0 : static_cast<int>(mKtxTexture->numLevels));
}

int TextureData::layers() const
{
    if (mTarget == QOpenGLTexture::TargetCubeMapArray)
        return static_cast<int>(mKtxTexture->numLayers / 6);

    return (isNull() ? 0 : static_cast<int>(mKtxTexture->numLayers));
}

int TextureData::faces() const
{
    return (isNull() ? 0 : static_cast<int>(mKtxTexture->numFaces));
}

uchar *TextureData::getWriteonlyData(int level, int layer, int face)
{
    if (isNull())
        return nullptr;

    if (mKtxTexture.use_count() > 1)
        create(target(), format(), width(),
            height(), depth(), layers(), samples(), levels());

    // generate mipmaps on next upload when level 0 is written
    mKtxTexture->generateMipmaps = (level == 0 &&
        canGenerateMipmaps(target(), format()) ? KTX_TRUE : KTX_FALSE);

    return const_cast<uchar*>(
        static_cast<const TextureData*>(this)->getData(level, layer, face));
}

const uchar *TextureData::getData(int level, int layer, int faceSlice) const
{
    if (isNull())
        return nullptr;
    
    auto offset = ktx_size_t{ };
    if (ktxTexture_GetImageOffset(ktxTexture(mKtxTexture.get()), 
            static_cast<ktx_uint32_t>(level),
            static_cast<ktx_uint32_t>(layer),
            static_cast<ktx_uint32_t>(faceSlice), &offset) == KTX_SUCCESS)
        return ktxTexture_GetData(ktxTexture(mKtxTexture.get())) + offset;

    return nullptr;
}

int TextureData::getImageSize(int level) const
{
    return (isNull() ? 0 :
      static_cast<int>(ktxTexture_GetImageSize(ktxTexture(mKtxTexture.get()),
          static_cast<ktx_uint32_t>(level)))) * std::max(depth() >> level, 1);
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
            std::memset(getWriteonlyData(level, layer, face),
                0x00, static_cast<size_t>(getImageSize(level)));
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

void TextureData::setPixelFormat(QOpenGLTexture::PixelFormat pixelFormat)
{
    if (!isNull())
        mKtxTexture->glFormat = static_cast<GLenum>(pixelFormat);
}

bool TextureData::upload(GLuint textureId,
    QOpenGLTexture::TextureFormat format)
{
    return upload(&textureId, format);
}

bool TextureData::upload(GLuint *textureId,
    QOpenGLTexture::TextureFormat format)
{
    if (isNull() || !textureId)
        return false;

    if (format) {
        const auto texelSizeA =
            getTextureDataSize(format) *
            getTextureComponentCount(format);
        const auto texelSizeB =
            getTextureDataSize(this->format()) *
            getTextureComponentCount(this->format());
        if (texelSizeA != texelSizeB)
            return false;
    }
    else {
        format = this->format();
    }

    if (isMultisampleTarget(mTarget)) {
        QOpenGLFunctions_3_3_Core gl;
        gl.initializeOpenGLFunctions();
        if (!*textureId)
            glGenTextures(1, textureId);

        return uploadMultisample(gl, *textureId, format);
    }

    Q_ASSERT(glGetError() == GL_NO_ERROR);

    const auto originalInternalFormat = mKtxTexture->glInternalformat;
    const auto originalFormat = mKtxTexture->glFormat;

    if (format != this->format()) {
        auto tmp = TextureData();
        tmp.create(QOpenGLTexture::Target::Target2D, format, 1, 1);
        mKtxTexture->glInternalformat = tmp.mKtxTexture->glInternalformat;
        mKtxTexture->glFormat = tmp.mKtxTexture->glFormat;
    }

    auto target = static_cast<GLenum>(mTarget);
    auto error = GLenum{ };
    const auto result = (ktxTexture_GLUpload(
        ktxTexture(mKtxTexture.get()), textureId, &target, &error) == KTX_SUCCESS);

    mKtxTexture->glInternalformat = originalInternalFormat;
    mKtxTexture->glFormat = originalFormat;

    Q_ASSERT(glGetError() == GL_NO_ERROR);
    return result;
}

bool TextureData::download(GLuint textureId)
{
    if (isNull() || !textureId)
        return false;

    Q_ASSERT(glGetError() == GL_NO_ERROR);
    QOpenGLFunctions_3_3_Core gl;
    gl.initializeOpenGLFunctions();

    if (isMultisampleTarget(mTarget))
        return downloadMultisample(gl, textureId);

    if (isCubemapTarget(mTarget))
        return downloadCubemap(gl, textureId);

    return download(gl, textureId);
}

bool TextureData::uploadMultisample(GL& gl, GLuint textureId,
    QOpenGLTexture::TextureFormat format)
{
    gl.glBindTexture(mTarget, textureId);
    if (mTarget == QOpenGLTexture::Target2DMultisample) {
        gl.glTexImage2DMultisample(mTarget, samples(),
            format, width(), height(), GL_FALSE);

        // upload single sample and resolve
        auto singleSampleTexture = *this;
        singleSampleTexture.mTarget = QOpenGLTexture::Target2D;
        singleSampleTexture.mSamples = 1;
        auto singleSampleTextureId = GLuint{ };
        const auto cleanup = qScopeGuard([&] { gl.glDeleteTextures(1, &singleSampleTextureId); });
        return (singleSampleTexture.upload(&singleSampleTextureId, format) &&
                resolveTexture(gl, singleSampleTextureId, textureId, width(), height(), format));
    }
    else {
        Q_ASSERT(mTarget == QOpenGLTexture::Target2DMultisampleArray);
        gl.glTexImage3DMultisample(mTarget, samples(),
            format, width(), height(), layers(), GL_FALSE);

        // TODO: upload
        Q_ASSERT(!"not implemented");
        return false;
    }
}

bool TextureData::download(GL& gl, GLuint textureId)
{
    gl.glBindTexture(mTarget, textureId);
    for (auto level = 0; level < levels(); ++level) {
        auto data = getWriteonlyData(level, 0, 0);
        if (mKtxTexture->isCompressed) {
            auto size = GLint{ };
            gl.glGetTexLevelParameteriv(mTarget, level,
                GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
            if (glGetError() != GL_NO_ERROR || size > getImageSize(level))
                return false;
            gl.glGetCompressedTexImage(mTarget, level, data);
        }
        else {
            gl.glGetTexImage(mTarget, level,
                pixelFormat(), pixelType(), data);
        }
    }
    return (glGetError() == GL_NO_ERROR);
}

bool TextureData::downloadCubemap(GL& gl, GLuint textureId)
{
    // TODO: download
    Q_ASSERT(!"not implemented");
    return true;
}

bool TextureData::downloadMultisample(GL& gl, GLuint textureId)
{
    if (mTarget == QOpenGLTexture::Target2DMultisample) {
        // create single sample texture (=upload), resolve, download and copy plane
        auto singleSampleTexture = *this;
        singleSampleTexture.mTarget = QOpenGLTexture::Target2D;
        singleSampleTexture.mSamples = 1;
        auto singleSampleTextureId = GLuint{ };
        const auto cleanup = qScopeGuard([&] { gl.glDeleteTextures(1, &singleSampleTextureId); });
        if (!singleSampleTexture.upload(&singleSampleTextureId) ||
            !resolveTexture(gl, textureId, singleSampleTextureId, width(), height(), format()) ||
            !singleSampleTexture.download(singleSampleTextureId))
            return false;

        std::memcpy(getWriteonlyData(0, 0, 0), singleSampleTexture.getData(0, 0, 0), getImageSize(0));
        return true;
    }
    else {
        Q_ASSERT(mTarget == QOpenGLTexture::Target2DMultisampleArray);
        // TODO: download
        Q_ASSERT(!"not implemented");
        return false;
    }
}


