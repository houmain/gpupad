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
    mFlipY = texture.flipY;

    context.usedItems += texture.id;
    if (!texture.fileName.isEmpty())
        mImages.push_back({ 0, 0, QOpenGLTexture::CubeMapPositiveX,
            texture.fileName, QImage() });

    for (const auto &item : texture.items)
        if (auto image = castItem<::Image>(item)) {
            context.usedItems += image->id;
            if (!image->fileName.isEmpty())
                mImages.push_back({ image->level, image->layer, image->face,
                    image->fileName, QImage() });
        }
}

bool GLTexture::operator==(const GLTexture &rhs) const
{
    return std::tie(mTarget, mFormat, mWidth,
                    mHeight, mDepth, mFlipY, mImages) ==
           std::tie(rhs.mTarget, rhs.mFormat,rhs.mWidth,
                    rhs.mHeight, rhs.mDepth, rhs.mFlipY, rhs.mImages);
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
    for (Image& image : mImages) {
        auto prevImage = image.image;
        if (!Singletons::fileCache().getImage(image.fileName, &image.image)) {
            messages.insert(MessageType::LoadingFileFailed, image.fileName);
            continue;
        }
        mSystemCopiesModified = (image.image != prevImage);
    }
}

void GLTexture::upload(RenderContext &context)
{
    Q_UNUSED(context);
    if (mTexture && !mSystemCopiesModified)
        return;

    mTexture = std::make_unique<QOpenGLTexture>(mTarget);
    mTexture->setSize(mWidth, mHeight, mDepth);
    mTexture->setFormat(mFormat);
    mTexture->setAutoMipMapGenerationEnabled(false);
    mTexture->setMipLevels(mTexture->maximumMipLevels());

    // TODO:
    if (mTarget == QOpenGLTexture::Target2DMultisample ||
        mTarget == QOpenGLTexture::Target2DMultisampleArray)
        mTexture->setSamples(4);

    mTexture->allocateStorage();

    for (const auto &image : mImages)
        if (!image.image.isNull() && image.level == 0)
            uploadImage(image);

    if (mTexture->mipLevels() > 1)
        mTexture->generateMipMaps();

    for (const auto &image : mImages)
        if (!image.image.isNull() && image.level != 0)
            uploadImage(image);

    mSystemCopiesModified = mDeviceCopiesModified = false;
}

void GLTexture::getImageDataFormat(QOpenGLTexture::PixelFormat *format,
                               QOpenGLTexture::PixelType *dataType) const
{
    *format = QOpenGLTexture::RGBA;
    *dataType = QOpenGLTexture::UInt8;
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
            *format = QOpenGLTexture::RGBA_Integer;
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
            *format = QOpenGLTexture::RGBA_Integer;
            *dataType = QOpenGLTexture::Int8;
            break;

        default:
            break;
    }
}

void GLTexture::uploadImage(const Image &image)
{
    auto sourceFormat = QOpenGLTexture::RGBA;
    auto sourceType = QOpenGLTexture::UInt8;
    getImageDataFormat(&sourceFormat, &sourceType);
    auto source = image.image.convertToFormat(QImage::Format_RGBA8888);
    source = source.scaled(
        std::max(mWidth >> image.level, 1),
        std::max(mHeight >> image.level, 1));
    if (mFlipY)
        source = source.mirrored();

    auto uploadOptions = QOpenGLPixelTransferOptions();
    uploadOptions.setAlignment(1);

    if (mTarget == QOpenGLTexture::TargetCubeMap) {
        mTexture->setData(image.level, image.layer, image.face,
            sourceFormat, sourceType, source.constBits(),
            &uploadOptions);
    }
    else {
        mTexture->setData(image.level, image.layer,
            sourceFormat, sourceType, source.constBits(),
            &uploadOptions);
    }
}

bool GLTexture::download(RenderContext &context)
{
    Q_UNUSED(context);
    if (!mDeviceCopiesModified)
        return false;

    // generate mip maps when a sublevel image is attached
    auto sublevel = false;
    for (auto &image : mImages)
        sublevel |= (image.level != 0);
    if (sublevel && mTexture->mipLevels() > 1)
        mTexture->generateMipMaps();

    auto imageUpdated = false;
    for (auto &image : mImages)
        imageUpdated |= downloadImage(context, image);

    mSystemCopiesModified = mDeviceCopiesModified = false;
    return imageUpdated;
}

std::unique_ptr<QOpenGLTexture> resolveMultisampleTexture(
    RenderContext &context, GLTexture &source)
{
    auto createFBO = [&]() {
        auto fbo = GLuint{ };
        context.glGenFramebuffers(1, &fbo);
        return fbo;
    };
    auto freeFBO = [](GLuint fbo) {
        auto& gl = *QOpenGLContext::currentContext()->functions();
        gl.glDeleteFramebuffers(1, &fbo);
    };

    auto level = 0;
    auto width = source.width();
    auto height = source.height();
    auto format = source.format();

    auto attachment = GL_COLOR_ATTACHMENT0;
    auto blitMask = GL_COLOR_BUFFER_BIT;
    if (source.isDepthTexture()) {
        attachment = GL_DEPTH_ATTACHMENT;
        blitMask = GL_DEPTH_BUFFER_BIT;
    }
    else if (source.isSencilTexture()) {
        attachment = GL_STENCIL_ATTACHMENT;
        blitMask = GL_STENCIL_BUFFER_BIT;
    }
    else if (source.isDepthSencilTexture()) {
        attachment = GL_DEPTH_STENCIL_ATTACHMENT;
        blitMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    }

    auto sourceFbo = GLObject(createFBO(), freeFBO);
    context.glBindFramebuffer(GL_FRAMEBUFFER, sourceFbo);
    context.glFramebufferTexture(GL_FRAMEBUFFER, attachment,
        source.getReadOnlyTextureId(context), level);

    auto destFbo = GLObject(createFBO(), freeFBO);
    context.glBindFramebuffer(GL_FRAMEBUFFER, destFbo);
    auto dest = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    dest->setFormat(format);
    dest->setSize(width, height);
    dest->allocateStorage();
    context.glFramebufferTexture(GL_FRAMEBUFFER, attachment,
        dest->textureId(), level);

    context.glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFbo);
    context.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFbo);
    context.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, blitMask, GL_NEAREST);
    context.glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
    return dest;
}

bool GLTexture::downloadImage(RenderContext &context, Image& image)
{
    auto dest = QImage();
    auto width = std::max(mWidth >> image.level, 1);
    auto height = std::max(mHeight >> image.level, 1);
    auto format = QOpenGLTexture::RGBA;
    auto dataType = QOpenGLTexture::UInt8;
    getImageDataFormat(&format, &dataType);

    if (isDepthTexture()) {
        format = QOpenGLTexture::Depth;
        dataType = QOpenGLTexture::UInt8;
        dest = QImage(width, height, QImage::Format_Grayscale8);
    }
    else {
        dest = QImage(width, height, QImage::Format_RGBA8888);
    }

    // TODO: move. resolve level 0 first, then generate mip maps...
    if (mTarget == QOpenGLTexture::Target2DMultisample) {
        auto resolved = resolveMultisampleTexture(context, *this);
        resolved->bind();
        context.glGetTexImage(resolved->target(), image.level,
            format, dataType, dest.bits());
        resolved->release();
    }
    else if (context.gl45) {
        auto layer = image.layer;
        if (mTarget == Texture::Target::TargetCubeMapArray)
            layer *= 6;
        if (mTarget == Texture::Target::TargetCubeMap ||
            mTarget == Texture::Target::TargetCubeMapArray)
            layer += (image.face - QOpenGLTexture::CubeMapPositiveX);

        context.gl45->glGetTextureSubImage(mTexture->textureId(),
            image.level, 0, 0, layer, width, height, 1,
            format, dataType, dest.byteCount(), dest.bits());
    }
    else if (mTarget == QOpenGLTexture::Target1D ||
             mTarget == QOpenGLTexture::Target2D ||
             mTarget == QOpenGLTexture::TargetRectangle) {
        mTexture->bind();
        context.glGetTexImage(mTarget, image.level,
            format, dataType, dest.bits());
        mTexture->release();
    }
    else {
        context.messages.insert(MessageType::Error,
            "downloading image failed");
        return false;
    }

    if (mFlipY)
        dest = dest.mirrored();

    if (image.image == dest)
        return false;
    image.image = dest;
    return true;
}

