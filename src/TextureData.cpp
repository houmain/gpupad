#include "TextureData.h"
#include "session/Item.h"
#include <cstring>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>

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
            case QImage::Format_Grayscale16:
                return QOpenGLTexture::R16_UNorm;
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

    bool canGenerateMipmaps(const QOpenGLTexture::TextureFormat &format)
    {
        const auto dataType = getTextureDataType(format);
        return (dataType == TextureDataType::Normalized ||
                dataType == TextureDataType::Float);
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
    if (a.isSharedWith(b))
        return true;

    if (a.target() != b.target() ||
        a.format() != b.format() ||
        a.width() != b.width() ||
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
                if (sa != sb ||
                    std::memcmp(da, db, static_cast<size_t>(sa)) != 0)
                    return false;
            }
    return true;
}

bool TextureData::isSharedWith(const TextureData &other) const
{
    return (mKtxTexture == other.mKtxTexture &&
            mTarget == other.mTarget);
}

bool operator!=(const TextureData &a, const TextureData &b)
{
    return !(a == b);
}

bool TextureData::create(
    QOpenGLTexture::Target target,
    QOpenGLTexture::TextureFormat format,
    int width, int height, int depth, int layers)
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
            [[fallthrough]];
        case QOpenGLTexture::TargetCubeMap:
            createInfo.baseHeight = static_cast<ktx_uint32_t>(height);
            createInfo.numDimensions = 3;
            createInfo.numFaces = 6;
            break;

        default:
            return false;
    }

    createInfo.numLevels = (!canGenerateMipmaps(format) ? 1 :
        getLevelCount(createInfo));

    auto texture = std::add_pointer_t<ktxTexture>{ };
    if (ktxTexture_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE,
            &texture) == KTX_SUCCESS) {
        mTarget = target;
        mKtxTexture.reset(texture, &ktxTexture_Destroy);
        clear();
        return true;
    }
    return false;
}

bool TextureData::load(const QString &fileName) 
{
    auto texture = std::add_pointer_t<ktxTexture>{ };
    if (ktxTexture_CreateFromNamedFile(fileName.toUtf8().constData(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) == KTX_SUCCESS) {
        mTarget = getTarget(*texture);
        mKtxTexture.reset(texture, &ktxTexture_Destroy);
        return true;
    }

    auto image = QImage();
    if (!image.load(fileName))
        return false;
    image.convertTo(getNextNativeImageFormat(image.format()));
    if (!create(QOpenGLTexture::Target2D, getTextureFormat(image.format()),
          image.width(), image.height(), 1, 1))
        return false;
    if (static_cast<int>(image.sizeInBytes()) != getLevelSize(0))
        return false;

    std::memcpy(getWriteonlyData(0, 0, 0), image.constBits(),
        static_cast<size_t>(getLevelSize(0)));
    return true;
}

bool TextureData::save(const QString &fileName) const 
{
    if (fileName.toLower().endsWith(".ktx") &&
        ktxTexture_WriteToNamedFile(mKtxTexture.get(), 
            fileName.toUtf8().constData()))
        return true;

    auto image = toImage();
    if (image.isNull())
        return false;

    return image.save(fileName);
}

bool TextureData::isNull() const 
{
    return (mKtxTexture == nullptr);
}

QImage TextureData::toImage() const 
{
    const auto imageFormat = getImageFormat(pixelFormat(), pixelType());
    if (imageFormat == QImage::Format_Invalid)
        return { };
    auto image = QImage(width(), height(), imageFormat);
    if (static_cast<int>(image.sizeInBytes()) != getLevelSize(0))
        return { };
    std::memcpy(image.bits(), getData(0, 0, 0),
        static_cast<size_t>(getLevelSize(0)));

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
        create(target(), format(), width(), height(), depth(), layers());

    // generate mipmaps on next upload when level 0 is written
    mKtxTexture->generateMipmaps =
        (level == 0 && canGenerateMipmaps(format()) ? KTX_TRUE : KTX_FALSE);

    return const_cast<uchar*>(
        static_cast<const TextureData*>(this)->getData(level, layer, face));
}

const uchar *TextureData::getData(int level, int layer, int face) const
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

int TextureData::getLevelSize(int level) const
{
    return (isNull() ? 0 :
      static_cast<int>(ktxTexture_GetImageSize(mKtxTexture.get(),
          static_cast<ktx_uint32_t>(level))));
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

#if defined(_WIN32)
    initializeKtxOpenGLFunctions();
#endif

    Q_ASSERT(glGetError() == GL_NO_ERROR);
    auto target = static_cast<GLenum>(mTarget);
    auto error = GLenum{ };

    const auto originalFormat = mKtxTexture->glInternalformat;
    if (format)
        mKtxTexture->glInternalformat = static_cast<ktx_uint32_t>(format);

    const auto result = (ktxTexture_GLUpload(
        mKtxTexture.get(), textureId, &target, &error) == KTX_SUCCESS);

    mKtxTexture->glInternalformat = originalFormat;
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
    gl.glBindTexture(mTarget, textureId);

    for (auto level = 0; level < levels(); ++level)
        for (auto layer = 0; layer < layers(); ++layer)
            for (auto face = 0; face < faces(); ++face) {
                auto data = getWriteonlyData(level, layer, face);
                if (mKtxTexture->isCompressed) {
                    auto size = GLint{ };
                    gl.glGetTexLevelParameteriv(mTarget, level,
                        GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
                    if (size > getLevelSize(level))
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

void TextureData::clear()
{
    if (isNull())
        return;

    const auto level = 0;
    for (auto layer = 0; layer < layers(); ++layer)
        for (auto face = 0; face < faces(); ++face)
            std::memset(getWriteonlyData(level, layer, face),
                0xFF, static_cast<size_t>(getLevelSize(level)));
}
