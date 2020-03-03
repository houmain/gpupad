#include "TextureData.h"
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

bool operator==(const TextureData &a, const TextureData &b) 
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
    createInfo.baseHeight = static_cast<ktx_uint32_t>(height);
    createInfo.baseDepth = static_cast<ktx_uint32_t>(depth);
    createInfo.numLayers = static_cast<ktx_uint32_t>(layers);
    createInfo.numFaces = 1;
    createInfo.numLevels = 1;
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_TRUE;

    switch (target) {
      case QOpenGLTexture::Target1DArray:
        createInfo.isArray = KTX_TRUE;
        [[fallthrough]];
      case QOpenGLTexture::Target1D:
        createInfo.numDimensions = 1;
        break;

      case QOpenGLTexture::Target2DArray:
        createInfo.isArray = KTX_TRUE;
        [[fallthrough]];
      case QOpenGLTexture::Target2D:
      case QOpenGLTexture::TargetRectangle:
        createInfo.numDimensions = 2;
        break;

      case QOpenGLTexture::Target3D:
        createInfo.numDimensions = 3;
        break;

      case QOpenGLTexture::TargetCubeMapArray:
        createInfo.isArray = KTX_TRUE;
        [[fallthrough]];
      case QOpenGLTexture::TargetCubeMap:
        createInfo.numDimensions = 3;
        createInfo.numFaces = 6;
        break;

      default:
        return false;
    }

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
        mKtxTexture.reset(texture, &ktxTexture_Destroy);
        return true;
    }

    auto image = QImage();
    if (!image.load(fileName))
        return false;
    if (!create(QOpenGLTexture::Target2D, getTextureFormat(image.format()),
          image.width(), image.height(), 1, 1))
        return false;
    if (static_cast<int>(image.sizeInBytes()) != getLevelSize(0))
        return false;

    if (format() == QOpenGLTexture::RGB8_UNorm ||
        format() == QOpenGLTexture::RGBA8_UNorm)
        image = std::move(image).rgbSwapped();

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
    if (format() == QOpenGLTexture::RGB8_UNorm ||
        format() == QOpenGLTexture::RGBA8_UNorm)
        image = std::move(image).rgbSwapped();

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

bool TextureData::upload(GLuint textureId)
{
    return upload(&textureId);
}

bool TextureData::upload(GLuint *textureId)
{
    if (isNull() || !textureId)
        return false;

#if defined(_WIN32)
    initializeKtxOpenGLFunctions();
#endif

    auto target = static_cast<GLenum>(mTarget);
    auto error = GLenum{ };
    return (ktxTexture_GLUpload(
        mKtxTexture.get(), textureId, &target, &error) == KTX_SUCCESS);
}

bool TextureData::download(GLuint textureId)
{
    if (isNull() || !textureId)
        return false;

    QOpenGLFunctions_3_3_Core gl;
    gl.initializeOpenGLFunctions();
    gl.glBindTexture(mTarget, textureId);

    for (auto level = 0; level < levels(); ++level)
        for (auto layer = 0; layer < layers(); ++layer)
            for (auto face = 0; face < faces(); ++face) {
                auto data = getWriteonlyData(level, layer, face);
                switch (mTarget) {
                    case QOpenGLTexture::Target1D:
                    case QOpenGLTexture::Target2D:
                    case QOpenGLTexture::TargetRectangle:
                        gl.glGetTexImage(mTarget, level,
                            pixelFormat(), pixelType(), data);
                        break;

                    default:
                        return false;
                }
            }
    return true;
}

void TextureData::clear()
{
    if (isNull())
        return;

    for (auto level = 0; level < levels(); ++level)
        for (auto layer = 0; layer < layers(); ++layer)
            for (auto face = 0; face < faces(); ++face)
                std::memset(getWriteonlyData(level, layer, face), 
                    0xFF, static_cast<size_t>(getLevelSize(level)));
}
