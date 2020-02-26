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
                    image->face, image->fileName, ImageData() });
        }

    if (!texture.fileName.isEmpty() || mImages.empty())
        mImages.push_front({ texture.id, 0, 0, QOpenGLTexture::CubeMapPositiveX,
            texture.fileName, ImageData() });
}

bool operator==(const GLTexture::Image &a, const GLTexture::Image &b)
{
    return std::tie(a.level, a.layer, a.face, a.fileName) ==
           std::tie(b.level, b.layer, b.face, b.fileName);
}

bool GLTexture::operator==(const GLTexture &rhs) const
{
    return std::tie(mTarget, mFormat, mWidth, mHeight,
                    mDepth, mSamples, mImages,
                    mMultisampleTarget) ==
           std::tie(rhs.mTarget, rhs.mFormat, rhs.mWidth, rhs.mHeight,
                    rhs.mDepth, rhs.mSamples, rhs.mImages,
                    rhs.mMultisampleTarget);
}

GLuint GLTexture::getReadOnlyTextureId()
{
    reload();
    createTexture();
    upload();
    return (mMultisampleTexture ? mMultisampleTexture : mTexture)->textureId();
}

GLuint GLTexture::getReadWriteTextureId()
{
    reload();
    createTexture();
    upload();
    mDeviceCopiesModified = true;
    return (mMultisampleTexture ? mMultisampleTexture : mTexture)->textureId();
}

bool GLTexture::canUpdatePreview() const
{
    if (mMultisampleTarget != Texture::Target::Target2D)
        return false;

    if (mImages.size() != 1 || mImages[0].level != 0)
        return false;

    switch (mFormat) {
        case Texture::Format::R8_UNorm:
        case Texture::Format::RG8_UNorm:
        case Texture::Format::RGB8_UNorm:
        case Texture::Format::RGBA8_UNorm:
        case Texture::Format::R16_UNorm:
        case Texture::Format::RG16_UNorm:
        case Texture::Format::RGB16_UNorm:
        case Texture::Format::RGBA16_UNorm:
        case Texture::Format::R8_SNorm:
        case Texture::Format::RG8_SNorm:
        case Texture::Format::RGB8_SNorm:
        case Texture::Format::RGBA8_SNorm:
        case Texture::Format::R16_SNorm:
        case Texture::Format::RG16_SNorm:
        case Texture::Format::RGB16_SNorm:
        case Texture::Format::RGBA16_SNorm:
        case Texture::Format::R16F:
        case Texture::Format::RG16F:
        case Texture::Format::RGB16F:
        case Texture::Format::RGBA16F:
        case Texture::Format::R32F:
        case Texture::Format::RG32F:
        case Texture::Format::RGB32F:
        case Texture::Format::RGBA32F:
            return true;
        default:
            return false;
    }
}

QMap<ItemId, ImageData> GLTexture::getModifiedImages()
{
    if (!download())
        return { };

    auto result = QMap<ItemId, ImageData>();
    foreach (const Image &image, mImages)
        result[image.itemId] = image.image;

    return result;
}

void GLTexture::clear(QColor color, double depth, int stencil)
{
    auto &gl = GLContext::currentContext();
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
        gl.glClearColor(
            static_cast<float>(color.redF()),
            static_cast<float>(color.greenF()),
            static_cast<float>(color.blueF()),
            static_cast<float>(color.alphaF()));
        gl.glClear(GL_COLOR_BUFFER_BIT);
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
}

void GLTexture::copy(GLTexture &source)
{
    source.getReadOnlyTextureId();
    getReadWriteTextureId();
    resolveMultisampleTexture(*source.mTexture, *mTexture, 0);
}

void GLTexture::generateMipmaps()
{
    getReadWriteTextureId();
    if (mTexture->mipLevels() > 1)
        mTexture->generateMipMaps();
}

void GLTexture::reload()
{
    for (Image& image : mImages)
        if (!FileDialog::isEmptyOrUntitled(image.fileName)) {
            auto prevImage = image.image;
            if (!Singletons::fileCache().getImage(image.fileName, &image.image)) {
                mMessages += MessageList::insert(image.itemId,
                    MessageType::LoadingFileFailed, image.fileName);
                continue;
            }
            mSystemCopiesModified |= (image.image != prevImage);
        }
}

void GLTexture::createTexture()
{
    if (mTexture)
        return;

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

    foreach (const Image &image, mImages)
        if (!image.image.isNull() && image.level == 0)
            uploadImage(image);

    if (mTexture->mipLevels() > 1)
        mTexture->generateMipMaps();

    foreach (const Image &image, mImages)
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
    if (image.image.isNull()) {
        mMessages += MessageList::insert(
            image.itemId, MessageType::UploadingImageFailed);
        return;
    }
    const auto format = image.image.pixelFormat();
    const auto type = image.image.pixelType();
    const auto data = image.image.getData(0, 0, 0);

    if (mTarget == QOpenGLTexture::Target3D) {
        auto &gl = GLContext::currentContext();
        gl.glBindTexture(mTarget, mTexture->textureId());
        gl.glTexSubImage3D(mTarget, image.level, 0, 0, image.layer,
            getImageWidth(image.level), getImageHeight(image.level), 1,
            format, type, data);
    }
    else if (mTarget == QOpenGLTexture::TargetCubeMap ||
             mTarget == QOpenGLTexture::TargetCubeMapArray) {
        mTexture->setData(image.level, image.layer,
            image.face, format, type, data);
    }
    else {
        mTexture->setData(image.level, image.layer,
            format, type, data);
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
    if (!image.image.create(
            getImageWidth(image.level),
            getImageHeight(image.level), 
            mFormat)) {
        mMessages += MessageList::insert(
            image.itemId, MessageType::DownloadingImageFailed);
        return false;
    }
    const auto format = image.image.pixelFormat();
    const auto type = image.image.pixelType();
    const auto data = image.image.getWriteonlyData(0, 0, 0);

    if (mMultisampleTexture)
        resolveMultisampleTexture(*mMultisampleTexture,
            *mTexture, image.level);

    auto &gl = GLContext::currentContext();
    gl.glBindTexture(mTarget, mTexture->textureId());
    switch (mTarget) {
        case QOpenGLTexture::Target1D:
        case QOpenGLTexture::Target2D:
        case QOpenGLTexture::TargetRectangle:
            gl.glGetTexImage(mTarget, image.level, format, type, data);
            break;

        case QOpenGLTexture::Target1DArray:
            gl.glTexSubImage2D(mTarget, image.level, 0, image.layer,
                getImageWidth(image.level), 1,
                format, type, data);
            break;

        case QOpenGLTexture::Target2DArray:
        case QOpenGLTexture::Target3D:
            gl.glTexSubImage3D(mTarget, image.level, 0, 0, image.layer,
                getImageWidth(image.level), getImageHeight(image.level), 1,
                format, type, data);
            break;

        case QOpenGLTexture::TargetCubeMap:
            gl.glTexSubImage2D(image.face, image.level, 0, 0,
                getImageWidth(image.level), getImageHeight(image.level),
                format, type, data);
            break;

        case QOpenGLTexture::TargetCubeMapArray: {
            const auto layer = 6 * image.layer +
                (image.face - QOpenGLTexture::CubeMapPositiveX);
            gl.glTexSubImage3D(mTarget, image.level, 0, 0, layer,
                getImageWidth(image.level), getImageHeight(image.level), 1,
                format, type, data);
            break;
        }

        default:
            mMessages += MessageList::insert(
                image.itemId, MessageType::DownloadingImageFailed);
            return false;
    }
    return true;
}

GLObject GLTexture::createFramebuffer(GLuint textureId, int level) const
{
    auto &gl = GLContext::currentContext();
    auto createFBO = [&]() {
        auto fbo = GLuint{ };
        gl.glGenFramebuffers(1, &fbo);
        return fbo;
    };
    auto freeFBO = [](GLuint fbo) {
        auto &gl = GLContext::currentContext();
        gl.glDeleteFramebuffers(1, &fbo);
    };

    auto attachment = GLenum{ GL_COLOR_ATTACHMENT0 };
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
    gl.glBindFramebuffer(GL_FRAMEBUFFER,
        static_cast<GLuint>(prevFramebufferId));
    return fbo;
}

void GLTexture::resolveMultisampleTexture(QOpenGLTexture &source,
        QOpenGLTexture &dest, int level)
{
    auto &gl = GLContext::currentContext();
    const auto sourceFbo = createFramebuffer(source.textureId(), level);
    const auto destFbo = createFramebuffer(dest.textureId(), level);

    const auto width = getImageWidth(level);
    const auto height = getImageHeight(level);
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
    gl.glBindFramebuffer(GL_FRAMEBUFFER,
        static_cast<GLuint>(prevFramebufferId));
}
