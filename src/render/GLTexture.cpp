#include "GLTexture.h"
#include <QOpenGLPixelTransferOptions>

GLTexture::GLTexture(const Texture &texture)
    : mItemId(texture.id)
    , mFileName(texture.fileName)
    , mTarget(texture.target)
    , mFormat(texture.format)
    , mWidth(texture.width)
    , mHeight(texture.height)
    , mDepth(texture.depth)
    , mLayers(texture.layers)
    , mSamples(texture.samples)
    , mKind(getKind(texture))
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
}

bool GLTexture::operator==(const GLTexture &rhs) const
{
    return std::tie(mFileName, mTarget, mFormat, mWidth, mHeight, mDepth, mLayers, mSamples) ==
           std::tie(rhs.mFileName, rhs.mTarget, rhs.mFormat, rhs.mWidth, rhs.mHeight, rhs.mDepth, rhs.mLayers, rhs.mSamples);
}

GLuint GLTexture::getReadOnlyTextureId()
{
    reload(false);
    createTexture();
    upload();
    return (mMultisampleTexture ? mMultisampleTexture : mTextureObject);
}

GLuint GLTexture::getReadWriteTextureId()
{
    reload(true);
    createTexture();
    upload();
    mDeviceCopyModified = true;
    return (mMultisampleTexture ? mMultisampleTexture : mTextureObject);
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
    copyTexture(source.getReadOnlyTextureId(), getReadWriteTextureId(), 0);
}

void GLTexture::generateMipmaps()
{
    auto &gl = GLContext::currentContext();
    gl.glBindTexture(target(), getReadWriteTextureId());
    gl.glGenerateMipmap(target());
}

void GLTexture::reload(bool writeable)
{
    const auto prevData = mData;
    if (!FileDialog::isEmptyOrUntitled(mFileName))
        if (!Singletons::fileCache().getTexture(mFileName, &mData))
            mMessages += MessageList::insert(mItemId,
                MessageType::LoadingFileFailed, mFileName);

    // when writeable apply requested dimensions, otherwise keep file's
    const auto sameDimensions = [&]() {
        return (mFormat == mData.format() &&
                mWidth == mData.width() &&
                mHeight == mData.height() &&
                mDepth == mData.depth() &&
                mLayers == mData.layers());
    };
    if (mData.isNull() || (writeable && !sameDimensions()))
        mData.create(mTarget, mFormat, mWidth, mHeight, mDepth, mLayers);

    mSystemCopyModified |= (mData != prevData);
}

void GLTexture::createTexture()
{
    if (mTextureObject)
        return;

    auto &gl = GLContext::currentContext();
    const auto createTexture = [&]() {
        auto texture = GLuint{};
        gl.glGenTextures(1, &texture);
        return texture;
    };
    const auto freeTexture = [](GLuint texture) {
        auto &gl = GLContext::currentContext();
        gl.glDeleteTextures(1, &texture);
    };

    mTextureObject = GLObject(createTexture(), freeTexture);
    if (mMultisampleTarget != mTarget)
        mMultisampleTexture = GLObject(createTexture(), freeTexture);
}

void GLTexture::upload()
{
    if (!mSystemCopyModified)
        return;

    if (!mData.upload(mTextureObject)) {
        mMessages += MessageList::insert(
            mItemId, MessageType::UploadingImageFailed);
        return;
    }

    if (mMultisampleTexture)
        copyTexture(mTextureObject, mMultisampleTexture, 0);

    mSystemCopyModified = mDeviceCopyModified = false;
}

bool GLTexture::download()
{
    if (!mDeviceCopyModified)
        return false;

    if (mMultisampleTexture)
        copyTexture(mMultisampleTexture, mTextureObject, 0);

    if (!mData.download(mTextureObject)) {
        mMessages += MessageList::insert(
            mItemId, MessageType::DownloadingImageFailed);
        return false;
    }

    mSystemCopyModified = mDeviceCopyModified = false;
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

void GLTexture::copyTexture(GLuint sourceTextureId,
    GLuint destTextureId, int level)
{
    auto &gl = GLContext::currentContext();
    const auto sourceFbo = createFramebuffer(sourceTextureId, level);
    const auto destFbo = createFramebuffer(destTextureId, level);

    const auto width = mData.getLevelWidth(level);
    const auto height = mData.getLevelHeight(level);
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
