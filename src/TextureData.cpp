#include "TextureData.h"
#include "session/Item.h"
#include "tga/tga.h"
#include "gli/gli.hpp"
#include <cstring>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QScopeGuard>

#if defined(_WIN32)

// tweaked KTX glloader a bit to prevent glew dependency, look for //@ when updating
#  include "GL/glcorearb.h"
#  include "KTX/lib/gl_funcptrs.h"

static void initializeKtxOpenGLFunctions() {
  if (pfGlTexImage1D)
    return;
  auto gl = QOpenGLContext::currentContext();
#define ADD(X) pfGl##X = reinterpret_cast<decltype(pfGl##X)>(gl->getProcAddress("gl"#X))
  ADD(TexImage1D);
  ADD(TexImage3D);
  ADD(CompressedTexImage1D);
  ADD(CompressedTexImage2D);
  ADD(CompressedTexImage3D);
  ADD(GenerateMipmap);
  ADD(GetStringi);
#undef ADD
}
#endif // _WIN32

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
#if (QT_VERSION >= QT_VERSION_CHECK(5, 13, 0))
            case QImage::Format_Grayscale16:
                return QOpenGLTexture::R16_UNorm;
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

    QOpenGLTexture::Target getTarget(const ktxTexture &texture)
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
        const auto dataType = getTextureDataType(format);
        return (dataType == TextureDataType::Normalized ||
                dataType == TextureDataType::Float);
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

    gli::target getGliTarget(QOpenGLTexture::Target target)
    {
        switch (target) {
            case QOpenGLTexture::TargetBuffer:
            case QOpenGLTexture::Target1D: return gli::TARGET_1D;
            case QOpenGLTexture::Target1DArray: return gli::TARGET_1D_ARRAY;
            case QOpenGLTexture::Target2DMultisample:
            case QOpenGLTexture::Target2D: return gli::TARGET_2D;
            case QOpenGLTexture::Target2DMultisampleArray:
            case QOpenGLTexture::Target2DArray: return gli::TARGET_2D_ARRAY;
            case QOpenGLTexture::Target3D: return gli::TARGET_3D;
            case QOpenGLTexture::TargetCubeMap: return gli::TARGET_CUBE;
            case QOpenGLTexture::TargetCubeMapArray: return gli::TARGET_CUBE_ARRAY;
            case QOpenGLTexture::TargetRectangle: return gli::TARGET_RECT;
        }
        return { };
    }

    void flipImageVertically(uchar * data, int pitch, int height)
    {
        auto buffer = std::vector<std::byte>(pitch);
        auto low = data;
        auto high = &data[(height - 1) * pitch];
        for (; low < high; low += pitch, high -= pitch) {
            std::memcpy(buffer.data(), low, pitch);
            std::memcpy(low, high, pitch);
            std::memcpy(high, buffer.data(), pitch);
        }
      }
} // namespace

TextureDataType getTextureDataType(
    const QOpenGLTexture::TextureFormat &format)
{
    switch (format) {
        case QOpenGLTexture::R8_UNorm:
        case QOpenGLTexture::RG8_UNorm:
        case QOpenGLTexture::RGB8_UNorm:
        case QOpenGLTexture::RGBA8_UNorm:
        case QOpenGLTexture::R16_UNorm:
        case QOpenGLTexture::RG16_UNorm:
        case QOpenGLTexture::RGB16_UNorm:
        case QOpenGLTexture::RGBA16_UNorm:
        case QOpenGLTexture::R8_SNorm:
        case QOpenGLTexture::RG8_SNorm:
        case QOpenGLTexture::RGB8_SNorm:
        case QOpenGLTexture::RGBA8_SNorm:
        case QOpenGLTexture::R16_SNorm:
        case QOpenGLTexture::RG16_SNorm:
        case QOpenGLTexture::RGB16_SNorm:
        case QOpenGLTexture::RGBA16_SNorm:
        case QOpenGLTexture::RG3B2:
        case QOpenGLTexture::R5G6B5:
        case QOpenGLTexture::RGB5A1:
        case QOpenGLTexture::RGBA4:
            return TextureDataType::Normalized;

        case QOpenGLTexture::SRGB8:
        case QOpenGLTexture::SRGB8_Alpha8:
            return TextureDataType::Normalized_sRGB;

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
            return TextureDataType::Float;

        case QOpenGLTexture::R8U:
        case QOpenGLTexture::RG8U:
        case QOpenGLTexture::RGB8U:
        case QOpenGLTexture::RGBA8U:
        case QOpenGLTexture::S8:
            return TextureDataType::Uint8;

        case QOpenGLTexture::R16U:
        case QOpenGLTexture::RG16U:
        case QOpenGLTexture::RGB16U:
        case QOpenGLTexture::RGBA16U:
            return TextureDataType::Uint16;

        case QOpenGLTexture::R32U:
        case QOpenGLTexture::RG32U:
        case QOpenGLTexture::RGB32U:
        case QOpenGLTexture::RGBA32U:
            return TextureDataType::Uint32;

        case QOpenGLTexture::R8I:
        case QOpenGLTexture::RG8I:
        case QOpenGLTexture::RGB8I:
        case QOpenGLTexture::RGBA8I:
            return TextureDataType::Int8;

        case QOpenGLTexture::R16I:
        case QOpenGLTexture::RG16I:
        case QOpenGLTexture::RGB16I:
        case QOpenGLTexture::RGBA16I:
            return TextureDataType::Int16;

        case QOpenGLTexture::R32I:
        case QOpenGLTexture::RG32I:
        case QOpenGLTexture::RGB32I:
        case QOpenGLTexture::RGBA32I:
            return TextureDataType::Int32;

        case QOpenGLTexture::RGB10A2:
            return TextureDataType::Uint_10_10_10_2;

        default:
            return TextureDataType::Compressed;
    }
}

int getTextureDataSize(TextureDataType dataType)
{
    switch (dataType) {
        case TextureDataType::Normalized:
        case TextureDataType::Normalized_sRGB:
        case TextureDataType::Uint8:
        case TextureDataType::Int8:
            return 1;

        case TextureDataType::Uint16:
        case TextureDataType::Int16:
            return 2;

        case TextureDataType::Uint32:
        case TextureDataType::Int32:
        case TextureDataType::Float:
        case TextureDataType::Uint_10_10_10_2:
            return 4;

        case TextureDataType::Compressed:
            break;
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

    auto texture = std::add_pointer_t<ktxTexture>{ };
    if (ktxTexture_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE,
            &texture) == KTX_SUCCESS) {
        mKtxTexture.reset(texture, &ktxTexture_Destroy);
        mTarget = target;
        mSamples = (isMultisampleTarget(target) ? samples : 1);
        return true;
    }
    return false;
}

bool TextureData::loadKtx(const QString &fileName, bool flipVertically)
{
    auto texture = std::add_pointer_t<ktxTexture>{ };
    if (ktxTexture_CreateFromNamedFile(fileName.toUtf8().constData(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) != KTX_SUCCESS)
        return false;

    mTarget = getTarget(*texture);
    mKtxTexture.reset(texture, &ktxTexture_Destroy);
    mFlippedVertically = false;
    return true;
}

bool TextureData::loadGli(const QString &fileName, bool flipVertically) try
{
    auto texture = gli::load(fileName.toUtf8().constData());
    if (texture.empty())
        return false;

    if (flipVertically)
        texture = gli::flip(std::move(texture));

    auto gl = gli::gl(gli::gl::PROFILE_GL33);
    const auto format = gl.translate(texture.format(), texture.swizzles());
    const auto target = static_cast<QOpenGLTexture::Target>(gl.translate(texture.target()));
    const auto extent = texture.extent();
    const auto levels = (texture.levels() > 1 ? texture.levels() : 0);

    if (!create(target, static_cast<QOpenGLTexture::TextureFormat>(format.Internal),
          extent.x, extent.y, extent.z, static_cast<int>(texture.layers()), 1, 
          static_cast<int>(levels)))
        return false;

    for (auto layer = 0u; layer < texture.layers(); ++layer)
        for (auto face = 0u; face < texture.faces(); ++face)
            for (auto level = 0u; level < texture.levels(); ++level) {
                auto dest = getWriteonlyData(level, layer, face);
                const auto source = texture.data(layer, face, level);
                const auto sourceSize = static_cast<int>(texture.size(level));
                const auto destSize = getImageSize(level);
                if (!source || !dest)
                    return false;

                std::memcpy(dest, source, std::min(sourceSize, destSize));
            }
    mFlippedVertically = flipVertically;
    return true;
}
catch (...)
{
    return false;
}

bool TextureData::loadQImage(const QString &fileName, bool flipVertically)
{
    auto image = QImage();
    if (!image.load(fileName))
        return false;

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

bool TextureData::loadTga(const QString &fileName, bool flipVertically)
{
    auto f = std::fopen(fileName.toUtf8().constData(), "rb");
    if (!f)
        return false;
    auto guard = qScopeGuard([&]() { std::fclose(f); });

    auto file = tga::StdioFileInterface(f);
    auto decoder = tga::Decoder(&file);
    auto header = tga::Header();
    if (!decoder.readHeader(header))
        return false;

    const auto format = (header.bytesPerPixel() == 4 ? 
        QOpenGLTexture::TextureFormat::RGBA8_UNorm : 
        QOpenGLTexture::TextureFormat::R8_UNorm);
    if (!create(QOpenGLTexture::Target2D, format, header.width, header.height))
        return false;

    auto image = tga::Image();
    image.bytesPerPixel = header.bytesPerPixel();
    image.rowstride = header.width * header.bytesPerPixel();
    image.pixels = getWriteonlyData(0, 0, 0);
    if (static_cast<int>(image.rowstride * header.height) != getImageSize(0))
        return false;

    if (!decoder.readImage(header, image, nullptr))
        return false;

    if (flipVertically)
        flipImageVertically(image.pixels, image.rowstride, header.height);

    decoder.postProcessImage(header, image);
    mFlippedVertically = flipVertically;
    return true;
}

bool TextureData::load(const QString &fileName, bool flipVertically)
{
    return loadKtx(fileName, flipVertically) ||
           loadGli(fileName, flipVertically) ||
           loadTga(fileName, flipVertically) ||
           loadQImage(fileName, flipVertically);
}

bool TextureData::saveKtx(const QString &fileName, bool flipVertically) const
{
    if (!fileName.endsWith(".ktx", Qt::CaseInsensitive))
        return false;

    return (ktxTexture_WriteToNamedFile(
        mKtxTexture.get(), fileName.toUtf8().constData()) == KTX_SUCCESS);
}

bool TextureData::saveGli(const QString &fileName, bool flipVertically) const try
{
    if (!fileName.endsWith(".ktx", Qt::CaseInsensitive) &&
        !fileName.endsWith(".dds", Qt::CaseInsensitive) &&
        !fileName.endsWith(".kmg", Qt::CaseInsensitive))
        return false;

    auto gl = gli::gl(gli::gl::PROFILE_GL33);
    const auto gli_format = gl.find(
        static_cast<gli::gl::internal_format>(format()),
        static_cast<gli::gl::external_format>(pixelFormat()),
        static_cast<gli::gl::type_format>(pixelType()));
    if (!gli_format)
        return false;

    auto texture = gli::texture(getGliTarget(target()),
        gli_format, gli::extent3d{ width(), height(), depth() },
        layers(), faces(), levels());

    if (flipVertically)
        texture = gli::flip(std::move(texture));

    for (auto layer = 0u; layer < texture.layers(); ++layer)
        for (auto face = 0u; face < texture.faces(); ++face)
            for (auto level = 0u; level < texture.levels(); ++level) {
                auto dest = texture.data(layer, face, level);
                const auto source = getData(level, layer, face);
                const auto sourceSize = static_cast<int>(texture.size(level));
                const auto destSize = getImageSize(level);
                if (!source || !dest)
                    return false;

                std::memcpy(dest, source, std::min(sourceSize, destSize));
            }

    return gli::save(texture, fileName.toUtf8().constData());
}
catch (...)
{
    return false;
}

bool TextureData::saveTga(const QString &fileName, bool flipVertically) const
{
    if (!fileName.endsWith(".tga", Qt::CaseInsensitive))
        return false;

    auto imageData = toImage();
    if (imageData.isNull())
        return false;

    if (flipVertically)
        imageData = std::move(imageData).mirrored();

    auto header = tga::Header();
    header.width = width();
    header.height = height();
    if (imageData.isGrayscale()) {
        imageData = std::move(imageData).convertToFormat(QImage::Format_Grayscale8);
        header.imageType = tga::UncompressedGray;
        header.bitsPerPixel = 8;
    }
    else if (imageData.hasAlphaChannel()) {
        imageData = std::move(imageData).convertToFormat(QImage::Format_RGBA8888);
        header.imageType = tga::UncompressedRgb;
        header.bitsPerPixel = 32;
    }
    else {
        imageData = std::move(imageData).convertToFormat(QImage::Format_RGB888);
        header.imageType = tga::UncompressedRgb;
        header.bitsPerPixel = 24;
    }
    auto image = tga::Image();
    image.bytesPerPixel = header.bytesPerPixel();
    image.rowstride = header.width * header.bytesPerPixel();
    image.pixels = imageData.bits();
    if (image.rowstride * header.height != imageData.sizeInBytes())
        return false;

    auto file = std::fopen(fileName.toUtf8().constData(), "wb");
    if (!file)
        return false;
    auto guard = qScopeGuard([&]() { std::fclose(file); });
    auto fileInterface = tga::StdioFileInterface(file);
    auto encoder = tga::Encoder(&fileInterface);
    encoder.writeHeader(header);
    encoder.writeImage(header, image, nullptr);
    encoder.writeFooter();
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
           saveGli(fileName, flipVertically) ||
           saveTga(fileName, flipVertically) ||
           saveQImage(fileName, flipVertically);
}

bool TextureData::isNull() const 
{
    return (mKtxTexture == nullptr);
}

QImage TextureData::toImage() const 
{
    if (depth() > 1 || layers() > 1 || isCubemap())
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
    if (ktxTexture_GetImageOffset(mKtxTexture.get(), 
            static_cast<ktx_uint32_t>(level),
            static_cast<ktx_uint32_t>(layer),
            static_cast<ktx_uint32_t>(faceSlice), &offset) == KTX_SUCCESS)
        return ktxTexture_GetData(mKtxTexture.get()) + offset;

    return nullptr;
}

int TextureData::getImageSize(int level) const
{
    return (isNull() ? 0 :
      static_cast<int>(ktxTexture_GetImageSize(mKtxTexture.get(),
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
    if (!format)
        format = this->format();

    if (isMultisampleTarget(mTarget)) {
        QOpenGLFunctions_3_3_Core gl;
        gl.initializeOpenGLFunctions();
        if (!*textureId)
            glGenTextures(1, textureId);

        return uploadMultisample(gl, *textureId, format);
    }

    Q_ASSERT(glGetError() == GL_NO_ERROR);
#if defined(_WIN32)
    initializeKtxOpenGLFunctions();
#endif

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
        mKtxTexture.get(), textureId, &target, &error) == KTX_SUCCESS);

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


