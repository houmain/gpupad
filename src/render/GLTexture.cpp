#include "GLTexture.h"
#include <QOpenGLPixelTransferOptions>

GLTexture::GLTexture(const Texture &texture, PrepareContext &context)
{
    mItemId = texture.id;
    mTarget = texture.target;
    mFormat = texture.format;
    mWidth = texture.width;
    mHeight = texture.height;
    mDepth = texture.depth;

    context.usedItems += texture.id;
    mImages.push_back({ 0, 0, QOpenGLTexture::CubeMapPositiveX,
        texture.fileName, QImage() });

    for (const auto &item : texture.items)
        if (auto image = castItem<::Image>(item)) {
            context.usedItems += image->id;
            mImages.push_back({ image->level, image->layer, image->face,
                image->fileName, QImage() });
        }
}

bool GLTexture::operator==(const GLTexture &rhs) const
{
    return std::tie(mTarget, mFormat, mWidth,
                    mHeight, mDepth, mImages) ==
           std::tie(rhs.mTarget, rhs.mFormat,rhs.mWidth,
                    rhs.mHeight, rhs.mDepth, rhs.mImages);
}

bool GLTexture::isDepthTexture() const
{
    return (mFormat == QOpenGLTexture::D16
         || mFormat == QOpenGLTexture::D24
         || mFormat == QOpenGLTexture::D32
         || mFormat == QOpenGLTexture::D32F);
}

bool GLTexture::isSencilTexture() const
{
    return (mFormat == QOpenGLTexture::S8);
}

bool GLTexture::isDepthSencilTexture() const
{
    return (mFormat == QOpenGLTexture::D24S8
         || mFormat == QOpenGLTexture::D32FS8X24);
}

GLuint GLTexture::getReadOnlyTextureId(RenderContext &context)
{
    load(context.messages);
    upload(context);
    return (mTexture ? mTexture->textureId() : GL_NONE);
}

GLuint GLTexture::getReadWriteTextureId(RenderContext &context)
{
    load(context.messages);
    upload(context);
    mDeviceCopiesModified = true;
    return (mTexture ? mTexture->textureId() : GL_NONE);
}

QList<std::pair<QString, QImage>> GLTexture::getModifiedImages(
    RenderContext &context)
{
    if (!download(context))
        return { };

    auto result = QList<std::pair<QString, QImage>>();
    for (const auto& image : mImages)
        result.push_back(std::make_pair(image.fileName, image.image));

    return result;
}

void GLTexture::load(MessageList &messages) {
    messages.setContext(mItemId);
    for (Image& image : mImages)
        if (image.image.isNull()) {
            if (!Singletons::fileCache().getImage(image.fileName, &image.image))
                messages.insert(MessageType::LoadingFileFailed, image.fileName);
            else
                mSystemCopiesModified = true;
        }
}

void GLTexture::upload(RenderContext &context)
{
    Q_UNUSED(context);
    if (!mSystemCopiesModified)
        return;

    mTexture = std::make_unique<QOpenGLTexture>(mTarget);
    mTexture->setSize(mWidth, mHeight);
    mTexture->setFormat(mFormat);
    mTexture->setAutoMipMapGenerationEnabled(false);
    mTexture->setMipLevels(mTexture->maximumMipLevels());
    mTexture->allocateStorage();

    // TODO:
    auto image = mImages.front().image;
    if (!image.isNull()) {
        auto sourceFormat = QOpenGLTexture::RGBA;
        auto sourceType = QOpenGLTexture::UInt8;
        auto generateMipMaps = true;
        switch (mFormat) {
            case QOpenGLTexture::R8U:
            case QOpenGLTexture::RG8U:
            case QOpenGLTexture::RGB8U:
            case QOpenGLTexture::RGBA8U:
            case QOpenGLTexture::R16U:
            case QOpenGLTexture::RG16U:
            case QOpenGLTexture::RGB16U:
            case QOpenGLTexture::RGBA16U:
            case QOpenGLTexture::R32U:
            case QOpenGLTexture::RG32U:
            case QOpenGLTexture::RGB32U:
            case QOpenGLTexture::RGBA32U:
            case QOpenGLTexture::RGB10A2:
                sourceFormat = QOpenGLTexture::RGBA_Integer;
                generateMipMaps = false;
                break;

            case QOpenGLTexture::R8I:
            case QOpenGLTexture::RG8I:
            case QOpenGLTexture::RGB8I:
            case QOpenGLTexture::RGBA8I:
            case QOpenGLTexture::R16I:
            case QOpenGLTexture::RG16I:
            case QOpenGLTexture::RGB16I:
            case QOpenGLTexture::RGBA16I:
            case QOpenGLTexture::R32I:
            case QOpenGLTexture::RG32I:
            case QOpenGLTexture::RGB32I:
            case QOpenGLTexture::RGBA32I:
                sourceFormat = QOpenGLTexture::RGBA_Integer;
                sourceType = QOpenGLTexture::Int8;
                generateMipMaps = false;
                break;

            default:
                break;
        }

        image = image.convertToFormat(QImage::Format_RGBA8888);
        image = image.scaled(mWidth, mHeight);
        auto uploadOptions = QOpenGLPixelTransferOptions();
        uploadOptions.setAlignment(1);

        if (mTarget == QOpenGLTexture::TargetCubeMap) {
            // TODO: implement cube textures
            for (auto i = 0; i < 6; i++)
                mTexture->setData(0, 0,
                    static_cast<QOpenGLTexture::CubeMapFace>(
                        QOpenGLTexture::CubeMapPositiveX + i),
                    sourceFormat, sourceType, image.constBits(),
                    &uploadOptions);
        }
        else {
            mTexture->setData(0, sourceFormat, sourceType,
                image.constBits(), &uploadOptions);
        }
        if (generateMipMaps)
            mTexture->generateMipMaps();
    }
    mSystemCopiesModified = mDeviceCopiesModified = false;
}

bool GLTexture::download(RenderContext &context)
{
    Q_UNUSED(context);
    if (!mDeviceCopiesModified)
        return false;

    if (!mTexture)
        return false;

    auto format = QOpenGLTexture::RGBA;
    auto dataType = QOpenGLTexture::UInt8;
    auto image = QImage(mWidth, mHeight, QImage::Format_RGBA8888);

    if (isDepthTexture()) {
        format = QOpenGLTexture::Depth;
        dataType = QOpenGLTexture::UInt8;
        image = QImage(mWidth, mHeight, QImage::Format_Grayscale8);
    }

    mTexture->bind();
    context.glGetTexImage(mTarget, 0, format, dataType, image.bits());
    mTexture->release();

    // TODO:
    if (mImages.front().image == image)
        return false;
    mImages.front().image = image;

    mSystemCopiesModified = mDeviceCopiesModified = false;
    return true;
}
