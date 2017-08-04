#include "GLTexture.h"
#include <QOpenGLPixelTransferOptions>

GLTexture::GLTexture(const Texture &texture)
    : mKind(getKind(texture))
    , mTarget(texture.target)
    , mFormat(texture.format)
    , mWidth(texture.width)
    , mHeight(texture.height)
    , mDepth(texture.depth)
    , mLayers(texture.layers)
    , mSamples(texture.samples)
    , mFlipY(texture.flipY)
    , mMultisampleTarget(texture.target)
{
    if (mTarget == QOpenGLTexture::Target2DMultisample) {
        mMultisampleTarget = mTarget;
        mTarget = QOpenGLTexture::Target2D;
    }
    else if (mTarget == QOpenGLTexture::Target2DMultisampleArray) {
        mMultisampleTarget = mTarget;
        mTarget = QOpenGLTexture::Target2DArray;
    }

    mUsedItems += texture.id;

    for (const auto &item : texture.items)
        if (auto image = castItem<::Image>(item)) {
            mUsedItems += image->id;
            if (!image->fileName.isEmpty())
                mImages.push_back({ image->id, image->level, image->layer,
                    image->face, image->fileName, QImage() });
        }

    if (!texture.fileName.isEmpty() || mImages.empty())
        mImages.push_front({ texture.id, 0, 0, QOpenGLTexture::CubeMapPositiveX,
            texture.fileName, QImage() });
}

bool operator==(const GLTexture::Image &a, const GLTexture::Image &b)
{
    return std::tie(a.level, a.layer, a.face, a.fileName) ==
           std::tie(b.level, b.layer, b.face, b.fileName);
}

bool GLTexture::operator==(const GLTexture &rhs) const
{
    return std::tie(mTarget, mFormat, mWidth, mHeight,
                    mDepth, mSamples, mFlipY, mImages,
                    mMultisampleTarget) ==
           std::tie(rhs.mTarget, rhs.mFormat,rhs.mWidth, rhs.mHeight,
                    rhs.mDepth, rhs.mSamples, rhs.mFlipY, rhs.mImages,
                    rhs.mMultisampleTarget);
}

GLuint GLTexture::getReadOnlyTextureId()
{
    load();
    createTexture();
    upload();
    return (mMultisampleTexture ? mMultisampleTexture : mTexture)->textureId();
}

GLuint GLTexture::getReadWriteTextureId()
{
    load();
    createTexture();
    upload();
    mDeviceCopiesModified = true;
    return (mMultisampleTexture ? mMultisampleTexture : mTexture)->textureId();
}

QList<std::pair<ItemId, QImage>> GLTexture::getModifiedImages()
{
    if (!download())
        return { };

    auto result = QList<std::pair<ItemId, QImage>>();
    for (const auto& image : mImages)
        result.push_back(std::make_pair(image.itemId, image.image));

    return result;
}

void GLTexture::clear(QColor color, float depth, int stencil)
{
    auto& gl = GLContext::currentContext();
    auto fbo = createFramebuffer(getReadWriteTextureId(), 0);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    gl.glColorMask(true, true, true, true);
    gl.glDepthMask(true);
    gl.glStencilMask(0xFF);

    if (mKind.depth && mKind.stencil) {
        gl.glClearDepth(depth);
        gl.glClearStencil(stencil);
        gl.glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
    else if (mKind.depth) {
        gl.glClearDepth(depth);
        gl.glClear(GL_DEPTH_BUFFER_BIT);
    }
    else if (mKind.stencil) {
        gl.glClearStencil(stencil);
        gl.glClear(GL_STENCIL_BUFFER_BIT);
    }
    else {
        gl.glClearColor(color.redF(), color.greenF(),
            color.blueF(), color.alphaF());
        gl.glClear(GL_COLOR_BUFFER_BIT);
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
}

void GLTexture::generateMipmaps()
{
    getReadWriteTextureId();
    if (mTexture->mipLevels() > 1)
        mTexture->generateMipMaps();
}

void GLTexture::load()
{
    for (Image& image : mImages)
        if (!FileDialog::isEmptyOrUntitled(image.fileName)) {
            auto prevImage = image.image;
            if (!Singletons::fileCache().getImage(image.fileName, &image.image)) {
                mMessages += Singletons::messageList().insert(image.itemId,
                    MessageType::LoadingFileFailed, image.fileName);
                continue;
            }
            mSystemCopiesModified |= (image.image != prevImage);
        }
}

void GLTexture::getDataFormat(
    QOpenGLTexture::PixelFormat *format,
    QOpenGLTexture::PixelType *type) const
{
    switch (mFormat) {
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
        case QOpenGLTexture::RGB10A2:
        case QOpenGLTexture::RGBA16U:
        case QOpenGLTexture::RGBA32U:
            *format = QOpenGLTexture::RGBA_Integer;
            *type = QOpenGLTexture::UInt8;
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
        case QOpenGLTexture::RGBA16I:
        case QOpenGLTexture::RGBA32I:
            *format = QOpenGLTexture::RGBA_Integer;
            *type = QOpenGLTexture::Int8;
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

        default:
            *format = QOpenGLTexture::RGBA;
            *type = QOpenGLTexture::UInt8;
    }
}

QImage::Format GLTexture::getImageFormat(
    QOpenGLTexture::PixelFormat format,
    QOpenGLTexture::PixelType type) const
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

        case QOpenGLTexture::UInt32_D24S8:
            return QImage::Format_RGBA8888;

        default:
            return QImage::Format_Invalid;
    }
}

void GLTexture::createTexture()
{
    if (mTexture)
        return;

    auto format = QOpenGLTexture::PixelFormat();
    auto type = QOpenGLTexture::PixelType();
    getDataFormat(&format, &type);

    mTexture.reset(new QOpenGLTexture(mTarget));
    mTexture->setFormat(mFormat);
    mTexture->setSize(mWidth, mHeight, mDepth);
    if (mKind.array && mLayers > 1)
        mTexture->setLayers(mLayers);
    mTexture->setAutoMipMapGenerationEnabled(false);
    mTexture->setMipLevels(mKind.color ? mTexture->maximumMipLevels() : 1);
    mTexture->allocateStorage();

    if (mMultisampleTarget != mTarget) {
        mMultisampleTexture.reset(new QOpenGLTexture(mMultisampleTarget));
        mMultisampleTexture->setFormat(mFormat);
        mMultisampleTexture->setSize(mWidth, mHeight, mDepth);
        if (mKind.array && mLayers > 1)
            mMultisampleTexture->setLayers(mLayers);
        mMultisampleTexture->setSamples(mSamples);
        mMultisampleTexture->allocateStorage();
    }
}

void GLTexture::upload()
{
    if (!mSystemCopiesModified)
        return;

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

int GLTexture::getImageWidth(int level) const
{
    return std::max(mTexture->width() >> level, 1);
}

int GLTexture::getImageHeight(int level) const
{
    return std::max(mTexture->height() >> level, 1);
}

void GLTexture::uploadImage(const Image &image)
{
    auto format = QOpenGLTexture::PixelFormat();
    auto type = QOpenGLTexture::PixelType();
    getDataFormat(&format, &type);

    auto imageFormat = getImageFormat(format, type);
    if (imageFormat == QImage::Format_Invalid) {
        mMessages += Singletons::messageList().insert(
            image.itemId, MessageType::UploadingImageFailed);
        return;
    }
    auto source = image.image.convertToFormat(imageFormat);
    source = source.scaled(
        getImageWidth(image.level),
        getImageHeight(image.level));
    if (mFlipY)
        source = source.mirrored();

    if (mTarget == QOpenGLTexture::Target3D) {
        auto& gl = GLContext::currentContext();
        gl.glBindTexture(mTarget, mTexture->textureId());
        gl.glTexSubImage3D(mTarget, image.level,
            0, 0, image.layer,
            getImageWidth(image.level), getImageHeight(image.level), 1,
            format, type, source.constBits());
    }
    else if (mTarget == QOpenGLTexture::TargetCubeMap ||
             mTarget == QOpenGLTexture::TargetCubeMapArray) {
        mTexture->setData(image.level, image.layer,
            image.face, format, type, source.constBits());
    }
    else {
        mTexture->setData(image.level, image.layer,
            format, type, source.constBits());
    }

    if (mMultisampleTexture)
        resolveMultisampleTexture(*mTexture,
            *mMultisampleTexture, image.level);
}

bool GLTexture::download()
{
    if (!mDeviceCopiesModified)
        return false;

    auto imageUpdated = false;
    for (auto &image : mImages)
        imageUpdated |= downloadImage(image);

    mSystemCopiesModified = mDeviceCopiesModified = false;
    return imageUpdated;
}

bool GLTexture::downloadImage(Image& image)
{
    auto format = QOpenGLTexture::PixelFormat();
    auto type = QOpenGLTexture::PixelType();
    getDataFormat(&format, &type);
    auto width = getImageWidth(image.level);
    auto height = getImageHeight(image.level);

    auto imageFormat = getImageFormat(format, type);
    if (imageFormat == QImage::Format_Invalid) {
        mMessages += Singletons::messageList().insert(
            image.itemId, MessageType::DownloadingImageFailed);
        return false;
    }
    auto dest = QImage(width, height, imageFormat);

    if (mMultisampleTexture)
        resolveMultisampleTexture(*mMultisampleTexture,
            *mTexture, image.level);

    auto& gl = GLContext::currentContext();
    gl.glBindTexture(mTarget, mTexture->textureId());
    switch (mTarget) {
        case QOpenGLTexture::Target1D:
        case QOpenGLTexture::Target2D:
        case QOpenGLTexture::TargetRectangle:
            gl.glGetTexImage(mTarget, image.level, format, type, dest.bits());
            break;

        case QOpenGLTexture::Target1DArray:
            gl.glTexSubImage2D(mTarget, image.level,
                0, image.layer,
                getImageWidth(image.level), 1,
                format, type, dest.bits());
            break;

        case QOpenGLTexture::Target2DArray:
        case QOpenGLTexture::Target3D:
            gl.glTexSubImage3D(mTarget, image.level,
                0, 0, image.layer,
                getImageWidth(image.level), getImageHeight(image.level), 1,
                format, type, dest.bits());
            break;

        case QOpenGLTexture::TargetCubeMap:
            gl.glTexSubImage2D(image.face, image.level,
                0, 0,
                getImageWidth(image.level), getImageHeight(image.level),
                format, type, dest.bits());
            break;

        case QOpenGLTexture::TargetCubeMapArray: {
            auto layer = 6 * image.layer +
                (image.face - QOpenGLTexture::CubeMapPositiveX);
            gl.glTexSubImage3D(mTarget, image.level,
                0, 0, layer,
                getImageWidth(image.level), getImageHeight(image.level), 1,
                format, type, dest.bits());
            break;
        }

        default:
            mMessages += Singletons::messageList().insert(
                image.itemId, MessageType::DownloadingImageFailed);
            return false;
    }

    if (mFlipY)
        dest = dest.mirrored();

    if (image.image == dest)
        return false;
    image.image = dest;
    return true;
}

GLObject GLTexture::createFramebuffer(GLuint textureId, int level) const
{
    auto& gl = GLContext::currentContext();
    auto createFBO = [&]() {
        auto fbo = GLuint{ };
        gl.glGenFramebuffers(1, &fbo);
        return fbo;
    };
    auto freeFBO = [](GLuint fbo) {
        auto& gl = GLContext::currentContext();
        gl.glDeleteFramebuffers(1, &fbo);
    };

    auto attachment = GL_COLOR_ATTACHMENT0;
    if (mKind.depth && mKind.stencil)
        attachment = GL_DEPTH_STENCIL_ATTACHMENT;
    else if (mKind.depth)
        attachment = GL_DEPTH_ATTACHMENT;
    else if (mKind.stencil)
        attachment = GL_STENCIL_ATTACHMENT;

    auto prevFramebufferId = GLint();
    gl.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFramebufferId);
    auto fbo = GLObject(createFBO(), freeFBO);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl.glFramebufferTexture(GL_FRAMEBUFFER, attachment, textureId, level);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, prevFramebufferId);
    return fbo;
}

void GLTexture::resolveMultisampleTexture(QOpenGLTexture &source,
        QOpenGLTexture &dest, int level)
{
    auto& gl = GLContext::currentContext();
    auto sourceFbo = createFramebuffer(source.textureId(), level);
    auto destFbo = createFramebuffer(dest.textureId(), level);

    auto width = getImageWidth(level);
    auto height = getImageHeight(level);
    auto blitMask = GLbitfield{ GL_COLOR_BUFFER_BIT };
    if (mKind.depth && mKind.stencil)
        blitMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    else if (mKind.depth)
        blitMask = GL_DEPTH_BUFFER_BIT;
    else if (mKind.stencil)
        blitMask = GL_STENCIL_BUFFER_BIT;

    auto prevFramebufferId = GLint();
    gl.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFramebufferId);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFbo);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFbo);
    gl.glBlitFramebuffer(0, 0, width, height, 0, 0,
        width, height, blitMask, GL_NEAREST);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, prevFramebufferId);
}
