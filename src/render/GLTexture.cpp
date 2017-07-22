#include "GLTexture.h"
#include <QOpenGLPixelTransferOptions>

GLTexture::GLTexture(const Texture &texture)
    : mType(texture.type())
    , mTarget(texture.target)
    , mFormat(texture.format)
    , mWidth(texture.width)
    , mHeight(texture.height)
    , mDepth(texture.depth)
    , mSamples(texture.samples)
    , mFlipY(texture.flipY)
{
    mUsedItems += texture.id;
    if (!texture.fileName.isEmpty())
        mImages.push_back({ texture.id, 0, 0, QOpenGLTexture::CubeMapPositiveX,
            texture.fileName, QImage() });

    for (const auto &item : texture.items)
        if (auto image = castItem<::Image>(item)) {
            mUsedItems += image->id;
            if (!image->fileName.isEmpty())
                mImages.push_back({ image->id, image->level, image->layer,
                    image->face, image->fileName, QImage() });
        }
}

bool operator==(const GLTexture::Image &a, const GLTexture::Image &b)
{
    return std::tie(a.level, a.layer, a.face) ==
           std::tie(b.level, b.layer, b.face);
}

bool GLTexture::operator==(const GLTexture &rhs) const
{
    return std::tie(mTarget, mFormat, mWidth, mHeight,
                    mDepth, mSamples, mFlipY, mImages) ==
           std::tie(rhs.mTarget, rhs.mFormat,rhs.mWidth, rhs.mHeight,
                    rhs.mDepth, rhs.mSamples, rhs.mFlipY, rhs.mImages);
}

GLuint GLTexture::getReadOnlyTextureId()
{
    load();
    upload();
    return (mTexture ? mTexture->textureId() : GL_NONE);
}

GLuint GLTexture::getReadWriteTextureId()
{
    load();
    upload();
    mDeviceCopiesModified = true;
    return (mTexture ? mTexture->textureId() : GL_NONE);
}

QList<std::pair<QString, QImage>> GLTexture::getModifiedImages()
{
    if (!download())
        return { };

    auto result = QList<std::pair<QString, QImage>>();
    for (const auto& image : mImages)
        result.push_back(std::make_pair(image.fileName, image.image));

    return result;
}

void GLTexture::clear(QVariantList value)
{
    auto getField = [&](auto i) {
        return (i >= value.size() ? QVariant() : value[i]);
    };

    auto& gl = GLContext::currentContext();
    auto fbo = createFramebuffer(getReadWriteTextureId(), 0);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    if (mType == Texture::Type::Depth) {
        // TODO:
        //gl.glClearDepth(getField(0).toDouble());
        gl.glClear(GL_DEPTH_BUFFER_BIT);
    }
    else if (mType == Texture::Type::Stencil) {
        gl.glClearStencil(getField(0).toInt());
        gl.glClear(GL_STENCIL_BUFFER_BIT);
    }
    else if (mType == Texture::Type::DepthStencil) {
        // TODO:
        //gl.glClearDepth(getField(0).toDouble());
        gl.glClearStencil(getField(0).toInt());
        gl.glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
    else {
        gl.glClearColor(
            getField(0).toDouble(),
            getField(1).toDouble(),
            getField(2).toDouble(),
            getField(3).toDouble());
        gl.glClear(GL_COLOR_BUFFER_BIT);
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
}

void GLTexture::generateMipmaps()
{
    getReadWriteTextureId();
    mTexture->generateMipMaps();
}

void GLTexture::load()
{
    mMessages.clear();
    for (Image& image : mImages) {
        auto prevImage = image.image;
        if (!Singletons::fileCache().getImage(image.fileName, &image.image)) {
            mMessages += Singletons::messageList().insert(image.itemId,
                MessageType::LoadingFileFailed, image.fileName);
            continue;
        }
        mSystemCopiesModified |= (image.image != prevImage);
    }
}

void GLTexture::upload()
{
    if (!mTexture) {
        mTexture.reset(new QOpenGLTexture(mTarget));
        mTexture->setSize(mWidth, mHeight, mDepth);
        mTexture->setFormat(mFormat);

        if (mTarget == QOpenGLTexture::Target2DMultisample ||
            mTarget == QOpenGLTexture::Target2DMultisampleArray) {

            mTexture->setSamples(mSamples);

            auto resolveTarget = (mTarget == QOpenGLTexture::Target2DMultisample ?
                QOpenGLTexture::Target2D : QOpenGLTexture::Target2DArray);
            mResolveTexture.reset(new QOpenGLTexture(resolveTarget));
            mResolveTexture->setSize(mWidth, mHeight, mDepth);
            mResolveTexture->setFormat(mFormat);
            mResolveTexture->setAutoMipMapGenerationEnabled(false);
            mResolveTexture->setMipLevels(mResolveTexture->maximumMipLevels());
            mResolveTexture->allocateStorage();
        }
        else {
            mTexture->setAutoMipMapGenerationEnabled(false);
            mTexture->setMipLevels(mTexture->maximumMipLevels());
        }
        mTexture->allocateStorage();
    }

    if (mSystemCopiesModified) {
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

int GLTexture::getImageWidth(int level) const
{
    return std::max(mWidth >> level, 1);
}

int GLTexture::getImageHeight(int level) const
{
    return std::max(mHeight >> level, 1);
}

void GLTexture::uploadImage(const Image &image)
{
    auto sourceFormat = QOpenGLTexture::RGBA;
    auto sourceType = QOpenGLTexture::UInt8;
    getImageDataFormat(&sourceFormat, &sourceType);
    auto source = image.image.convertToFormat(QImage::Format_RGBA8888);

    source = source.scaled(
        getImageWidth(image.level),
        getImageHeight(image.level));

    if (mFlipY)
        source = source.mirrored();

    auto uploadOptions = QOpenGLPixelTransferOptions();
    uploadOptions.setAlignment(1);

    auto& texture = (mResolveTexture ? *mResolveTexture : *mTexture);
    if (mTarget == QOpenGLTexture::TargetCubeMap) {
        texture.setData(image.level, image.layer, image.face,
            sourceFormat, sourceType, source.constBits(),
            &uploadOptions);
    }
    else {
        texture.setData(image.level, image.layer,
            sourceFormat, sourceType, source.constBits(),
            &uploadOptions);
    }

    if (mResolveTexture)
        resolveMultisampleTexture(*mResolveTexture, *mTexture, image.level);
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
    auto& gl = GLContext::currentContext();
    auto dest = QImage();
    auto width = getImageWidth(image.level);
    auto height = getImageHeight(image.level);
    auto format = QOpenGLTexture::RGBA;
    auto dataType = QOpenGLTexture::UInt8;
    getImageDataFormat(&format, &dataType);
    if (mType == Texture::Type::Depth) {
        format = QOpenGLTexture::Depth;
        dataType = QOpenGLTexture::UInt8;
        dest = QImage(width, height, QImage::Format_Grayscale8);
    }
    else if (mType == Texture::Type::Stencil) {
        format = QOpenGLTexture::Stencil;
        dataType = QOpenGLTexture::UInt8;
        dest = QImage(width, height, QImage::Format_Grayscale8);
    }
    else if (mType == Texture::Type::DepthStencil) {
        format = QOpenGLTexture::DepthStencil;
        dataType = QOpenGLTexture::UInt32_D24S8;
        dest = QImage(width, height, QImage::Format_RGBA8888);
    }
    else {
        dest = QImage(width, height, QImage::Format_RGBA8888);
    }

    if (mResolveTexture)
        resolveMultisampleTexture(*mTexture, *mResolveTexture, image.level);

    auto& texture = (mResolveTexture ? *mResolveTexture : *mTexture);
    if (mTarget == QOpenGLTexture::Target1D ||
        mTarget == QOpenGLTexture::Target2D ||
        mTarget == QOpenGLTexture::TargetRectangle) {

        texture.bind();
        gl.glGetTexImage(mTarget, image.level, format, dataType, dest.bits());
        texture.release();
    }    
    else if (gl.v4_5) {
        auto layer = image.layer;
        if (mTarget == Texture::Target::TargetCubeMapArray)
            layer *= 6;
        if (mTarget == Texture::Target::TargetCubeMap ||
            mTarget == Texture::Target::TargetCubeMapArray)
            layer += (image.face - QOpenGLTexture::CubeMapPositiveX);

        gl.v4_5->glGetTextureSubImage(texture.textureId(),
            image.level, 0, 0, layer, width, height, 1,
            format, dataType, dest.byteCount(), dest.bits());
    }
    else {
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
    if (mType == Texture::Type::Depth)
        attachment = GL_DEPTH_ATTACHMENT;
    else if (mType == Texture::Type::Stencil)
        attachment = GL_STENCIL_ATTACHMENT;
    else if (mType == Texture::Type::DepthStencil)
        attachment = GL_DEPTH_STENCIL_ATTACHMENT;

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
    if (mType == Texture::Type::Depth)
        blitMask = GL_DEPTH_BUFFER_BIT;
    else if (mType == Texture::Type::Stencil)
        blitMask = GL_STENCIL_BUFFER_BIT;
    else if (mType == Texture::Type::DepthStencil)
        blitMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;

    auto prevFramebufferId = GLint();
    gl.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFramebufferId);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFbo);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFbo);
    gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, blitMask, GL_NEAREST);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, prevFramebufferId);
}
