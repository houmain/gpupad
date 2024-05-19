#include "GLTexture.h"
#include "GLBuffer.h"
#include "SynchronizeLogic.h"
#include "EvaluatedPropertyCache.h"
#include <QOpenGLPixelTransferOptions>
#include <cmath>

namespace {
    GLuint createFramebuffer(QOpenGLFunctions_3_3_Core &gl,
        GLenum target, GLuint textureId, GLenum attachment)
    {
        auto fbo = GLuint{ };
        gl.glGenFramebuffers(1, &fbo);
        gl.glBindFramebuffer(target, fbo);
        gl.glFramebufferTexture(target, attachment, textureId, 0);
        return fbo;
    }

    bool resolveTexture(QOpenGLFunctions_3_3_Core &gl,
        GLuint sourceTextureId, GLuint destTextureId, 
        int width, int height, QOpenGLTexture::TextureFormat format)
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
        const auto sourceFbo = createFramebuffer(gl,
            GL_READ_FRAMEBUFFER, sourceTextureId, attachment);
        const auto destFbo = createFramebuffer(gl,
            GL_DRAW_FRAMEBUFFER, destTextureId, attachment);
        gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, blitMask, GL_NEAREST);
        gl.glDeleteFramebuffers(1, &sourceFbo);
        gl.glDeleteFramebuffers(1, &destFbo);
        gl.glBindFramebuffer(GL_FRAMEBUFFER, previousTarget);
        return (glGetError() == GL_NONE);
    }

    bool uploadMultisample(QOpenGLFunctions_3_3_Core &gl,
        const TextureData &data, GLuint textureId)
    {
        gl.glBindTexture(data.target(), textureId);
        if (data.target() == QOpenGLTexture::Target2DMultisample) {
            gl.glTexImage2DMultisample(data.target(), data.samples(),
                data.format(), data.width(), data.height(), GL_FALSE);

            // upload single sample and resolve
            auto singleSampleTexture = data;
            singleSampleTexture.setTarget(QOpenGLTexture::Target2D);
            singleSampleTexture.setSamples(1);
            auto singleSampleTextureId = GLuint{ };
            const auto cleanup = qScopeGuard([&] { 
                gl.glDeleteTextures(1, &singleSampleTextureId);
            });
            return (singleSampleTexture.uploadGL(&singleSampleTextureId) &&
                    resolveTexture(gl, singleSampleTextureId, textureId, 
                        data.width(), data.height(), data.format()));
        }
        else {
            Q_ASSERT(data.target() == QOpenGLTexture::Target2DMultisampleArray);
            gl.glTexImage3DMultisample(data.target(), data.samples(),
                data.format(), data.width(), data.height(), data.layers(), GL_FALSE);

            // TODO: upload
        }
        Q_ASSERT(!"not implemented");
        return false;
    }

    bool download(QOpenGLFunctions_3_3_Core &gl, 
        TextureData &data, GLuint textureId)
    {
        gl.glBindTexture(data.target(), textureId);
        for (auto level = 0; level < data.levels(); ++level) {
            if (data.isCompressed()) {
                auto size = GLint{ };
                gl.glGetTexLevelParameteriv(data.target(), level,
                    GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
                if (glGetError() != GL_NO_ERROR || size > data.getImageSize(level))
                    return false;
                gl.glGetCompressedTexImage(data.target(), level, 
                    data.getWriteonlyData(level, 0, 0));
            }
            else {
                gl.glGetTexImage(data.target(), level,
                    data.pixelFormat(), data.pixelType(), 
                    data.getWriteonlyData(level, 0, 0));
            }
        }
        return (glGetError() == GL_NO_ERROR);
    }

    bool downloadCubemap(QOpenGLFunctions_3_3_Core &gl,
        TextureData &data, GLuint textureId)
    {
        // TODO: download
        Q_ASSERT(!"not implemented");
        return true;
    }

    bool downloadMultisample(QOpenGLFunctions_3_3_Core &gl,
        TextureData &data, GLuint textureId)
    {
        if (data.target() == QOpenGLTexture::Target2DMultisample) {
            // create single sample texture (=upload), resolve, download and copy plane
            auto singleSampleTexture = data;
            singleSampleTexture.setTarget(QOpenGLTexture::Target2D);
            singleSampleTexture.setSamples(1);
            auto singleSampleTextureId = GLuint{ };
            const auto cleanup = qScopeGuard([&] { 
                gl.glDeleteTextures(1, &singleSampleTextureId);
            });
            if (!singleSampleTexture.uploadGL(&singleSampleTextureId) ||
                !resolveTexture(gl, textureId, singleSampleTextureId, 
                    data.width(), data.height(), data.format()) ||
                !download(gl, singleSampleTexture, singleSampleTextureId))
                return false;

            std::memcpy(data.getWriteonlyData(0, 0, 0), 
                singleSampleTexture.getData(0, 0, 0), data.getImageSize(0));
            return true;
        }
        else {
            Q_ASSERT(data.target() == QOpenGLTexture::Target2DMultisampleArray);
            // TODO: download
        }
        Q_ASSERT(!"not implemented");
        return false;
    }

    GLObject createFramebuffer(QOpenGLFunctions_3_3_Core &gl,
        const TextureKind &kind, GLuint textureId, int level)
    {
        const auto createFBO = [&]() {
            auto fbo = GLuint{ };
            gl.glGenFramebuffers(1, &fbo);
            return fbo;
        };
        const auto freeFBO = [](GLuint fbo) {
            auto &gl = GLContext::currentContext();
            gl.glDeleteFramebuffers(1, &fbo);
        };

        auto attachment = GLenum{ GL_COLOR_ATTACHMENT0 };
        if (kind.depth && kind.stencil)
            attachment = GL_DEPTH_STENCIL_ATTACHMENT;
        else if (kind.depth)
            attachment = GL_DEPTH_ATTACHMENT;
        else if (kind.stencil)
            attachment = GL_STENCIL_ATTACHMENT;

        auto fbo = GLObject(createFBO(), freeFBO);
        gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        gl.glFramebufferTexture(GL_FRAMEBUFFER, attachment, textureId, level);
        auto status = gl.glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            fbo.reset();
        return fbo;
    }

    bool copyTexture(QOpenGLFunctions_3_3_Core &gl,
        const TextureKind &kind, GLuint sourceTextureId, 
        GLuint destTextureId, int width, int height, int level)
    {
        const auto sourceFbo = createFramebuffer(gl, kind, sourceTextureId, level);
        const auto destFbo = createFramebuffer(gl, kind, destTextureId, level);
        if (!sourceFbo || !destFbo)
            return false;

        auto blitMask = GLbitfield{ GL_COLOR_BUFFER_BIT };
        if (kind.depth && kind.stencil)
            blitMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
        else if (kind.depth)
            blitMask = GL_DEPTH_BUFFER_BIT;
        else if (kind.stencil)
            blitMask = GL_STENCIL_BUFFER_BIT;

        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFbo);
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFbo);
        gl.glBlitFramebuffer(0, 0, width, height, 0, 0,
            width, height, blitMask, GL_NEAREST);
        return true;
    }
} // namespace

bool GLTexture::upload(QOpenGLFunctions_3_3_Core &gl,
        const TextureData &data, GLuint* textureId)
{
    Q_ASSERT(textureId);
    if (data.isNull())
        return false;

    auto cleanup = qScopeGuard([&] { 
        gl.glDeleteTextures(1, textureId);
        *textureId = GL_NONE;
    });

    if (!*textureId) {
        gl.glGenTextures(1, textureId);
    }
    else {
        cleanup.dismiss();
    }

    if (isMultisampleTarget(data.target())) {
        if (!uploadMultisample(gl, data, *textureId))
            return false;
    }
    else {
        if (!data.uploadGL(textureId))
            return false;
    }
    cleanup.dismiss();
    return true;
}

bool GLTexture::download(QOpenGLFunctions_3_3_Core &gl,
    TextureData &data, GLuint textureId)
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);

    if (isMultisampleTarget(data.target()))
        return downloadMultisample(gl, data, textureId);

    if (isCubemapTarget(data.target()))
        return downloadCubemap(gl, data, textureId);

    return ::download(gl, data, textureId);
}

GLTexture::GLTexture(const Texture &texture, ScriptEngine &scriptEngine)
    : mItemId(texture.id)
    , mFileName(texture.fileName)
    , mFlipVertically(texture.flipVertically)
    , mTarget(texture.target)
    , mFormat(texture.format)
    , mSamples(texture.samples)
    , mKind(getKind(texture))
{
    Singletons::evaluatedPropertyCache().evaluateTextureProperties(
        texture, &mWidth, &mHeight, &mDepth, &mLayers, &scriptEngine);

    if (mKind.dimensions < 2)
        mHeight = 1;
    if (mKind.dimensions < 3)
        mDepth = 1;
    if (!mKind.array)
        mLayers = 1;

    mUsedItems += texture.id;
}

GLTexture::GLTexture(const Buffer &buffer,
        GLBuffer *textureBuffer, Texture::Format format,
        ScriptEngine &scriptEngine)
    : mItemId(buffer.id)
    , mTextureBuffer(textureBuffer)
    , mTarget(Texture::Target::TargetBuffer)
    , mFormat(format)
    , mWidth(getBufferSize(buffer, scriptEngine, mMessages))
    , mHeight(1)
    , mDepth(1)
    , mLayers(1)
    , mSamples(1)
    , mKind()
{
    mUsedItems += buffer.id;
}

bool GLTexture::operator==(const GLTexture &rhs) const
{
    return std::tie(mMessages, mFileName, mFlipVertically, mTextureBuffer, mTarget, mFormat, mWidth, mHeight, mDepth, mLayers, mSamples) ==
           std::tie(rhs.mMessages, rhs.mFileName, rhs.mFlipVertically, rhs.mTextureBuffer, rhs.mTarget, rhs.mFormat, rhs.mWidth, rhs.mHeight, rhs.mDepth, rhs.mLayers, rhs.mSamples);
}

GLuint GLTexture::getReadOnlyTextureId()
{
    reload(false);
    createTexture();
    upload();
    return mTextureObject;
}

GLuint GLTexture::getReadWriteTextureId()
{
    reload(true);
    createTexture();
    upload();
    mDeviceCopyModified = true;
    mMipmapsInvalidated = true;
    return mTextureObject;
}

bool GLTexture::clear(std::array<double, 4> color, double depth, int stencil)
{
    auto &gl = GLContext::currentContext();
    auto fbo = createFramebuffer(gl, mKind, getReadWriteTextureId(), 0);
    if (!fbo)
        return false;

    gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl.glColorMask(true, true, true, true);
    gl.glDepthMask(true);
    gl.glStencilMask(0xFF);

    auto succeeded = true;
    if (mKind.depth && mKind.stencil) {
        gl.glClearBufferfi(GL_DEPTH_STENCIL, 0,
            static_cast<float>(depth), stencil);
    }
    else if (mKind.depth) {
        const auto d = static_cast<float>(depth);
        gl.glClearBufferfv(GL_DEPTH, 0, &d);
    }
    else if (mKind.stencil) {
        gl.glClearBufferiv(GL_STENCIL, 0, &stencil);
    }
    else {
        const auto srgbToLinear = [](auto value) {
            if (value <= 0.0404482362771082)
                return value / 12.92;
            return std::pow((value + 0.055) / 1.055, 2.4);
        };
        const auto multiplyRGBA = [&](auto factor) {
            for (auto &c : color)
                c *= factor;
        };

        const auto sampleType = getTextureSampleType(mFormat);
        switch (sampleType) {
            case TextureSampleType::Normalized_sRGB:
            case TextureSampleType::Float:
                for (auto i = 0u; i < 3; ++i)
                    color[i] = srgbToLinear(color[i]);
                break;
            case TextureSampleType::Int8: multiplyRGBA(0x7F); break;
            case TextureSampleType::Int16: multiplyRGBA(0x7FFF); break;
            case TextureSampleType::Int32: multiplyRGBA(0x7FFFFFFF); break;
            case TextureSampleType::Uint8: multiplyRGBA(0xFF); break;
            case TextureSampleType::Uint16: multiplyRGBA(0xFFFF); break;
            case TextureSampleType::Uint32: multiplyRGBA(0xFFFFFFFF); break;
            case TextureSampleType::Uint_10_10_10_2:
                for (auto i = 0u; i < 3; ++i)
                    color[i] *= 1024.0;
                color[3] *= 4.0;
                break;
            default:
                break;
        }

        switch (sampleType) {
            case TextureSampleType::Normalized:
            case TextureSampleType::Normalized_sRGB:
            case TextureSampleType::Float: {
                const auto values = std::array<float, 4>{
                    static_cast<float>(color[0]),
                    static_cast<float>(color[1]),
                    static_cast<float>(color[2]),
                    static_cast<float>(color[3])
                };
                gl.glClearBufferfv(GL_COLOR, 0, values.data());
                break;
            }

            case TextureSampleType::Uint8:
            case TextureSampleType::Uint16:
            case TextureSampleType::Uint32:
            case TextureSampleType::Uint_10_10_10_2: {
                const auto values = std::array<GLuint, 4>{
                    static_cast<GLuint>(color[0]),
                    static_cast<GLuint>(color[1]),
                    static_cast<GLuint>(color[2]),
                    static_cast<GLuint>(color[3])
                };
                gl.glClearBufferuiv(GL_COLOR, 0, values.data());
                break;
            }

            case TextureSampleType::Int8:
            case TextureSampleType::Int16:
            case TextureSampleType::Int32: {
                const auto values = std::array<GLint, 4>{
                    static_cast<GLint>(color[0]),
                    static_cast<GLint>(color[1]),
                    static_cast<GLint>(color[2]),
                    static_cast<GLint>(color[3])
                };
                gl.glClearBufferiv(GL_COLOR, 0, values.data());
                break;
            }

            default:
                succeeded = false;
                break;
        }
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
    return succeeded;
}

bool GLTexture::copy(GLTexture &source)
{
    const auto level = 0;
    auto &gl = GLContext::currentContext();
    return copyTexture(gl, mKind, source.getReadOnlyTextureId(),
        getReadWriteTextureId(), 
        mData.getLevelWidth(level),
        mData.getLevelHeight(level), level);
}

bool GLTexture::swap(GLTexture &other)
{
    if (mTarget != other.mTarget || 
        mFormat != other.mFormat || 
        mWidth != other.mWidth || 
        mHeight != other.mHeight || 
        mDepth != other.mDepth || 
        mLayers != other.mLayers || 
        mSamples != other.mSamples)
        return false;

    std::swap(mData, other.mData);
    std::swap(mDataWritten, other.mDataWritten);
    std::swap(mTextureObject, other.mTextureObject);
    std::swap(mSystemCopyModified, other.mSystemCopyModified);
    std::swap(mDeviceCopyModified, other.mDeviceCopyModified);
    std::swap(mMipmapsInvalidated, other.mMipmapsInvalidated);
    return true;
}

bool GLTexture::updateMipmaps()
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);
    if (mMipmapsInvalidated) {
        if (mData.levels() > 1) {
            auto &gl = GLContext::currentContext();
            gl.glBindTexture(target(), getReadWriteTextureId());
            gl.glGenerateMipmap(target());
        }
        mMipmapsInvalidated = false;
    }
    return (glGetError() == GL_NO_ERROR);
}

void GLTexture::reload(bool forWriting)
{
    if (mTextureBuffer) {
        if (forWriting)
            mTextureBuffer->getReadWriteBufferId();
        return;
    }

    auto fileData = TextureData{ };
    if (!FileDialog::isEmptyOrUntitled(mFileName)) {
        if (!Singletons::fileCache().getTexture(mFileName, mFlipVertically, &fileData))
            mMessages += MessageList::insert(mItemId,
                MessageType::LoadingFileFailed, mFileName);

        const auto hasSameDimensions = [&](const TextureData &data) {
            return (mTarget == data.target() &&
                    mFormat == data.format() &&
                    mWidth == data.width() &&
                    mHeight == data.height() &&
                    mDepth == data.depth() &&
                    mLayers == data.layers());
        };

        // validate dimensions when writing
        if (!forWriting || hasSameDimensions(fileData)) {
            mSystemCopyModified |= !mData.isSharedWith(fileData);
            mData = fileData;
        }
    }

    if (mData.isNull()) {
        if (!mData.create(mTarget, mFormat, mWidth, mHeight, 
                mDepth, mLayers, mSamples)) {
            mData.create(mTarget, Texture::Format::RGBA8_UNorm, 
                1, 1, 1, 1, 1);
            mMessages += MessageList::insert(mItemId,
                MessageType::CreatingTextureFailed);
        }
        mData.clear();
        mSystemCopyModified = true;
    }
}

void GLTexture::createTexture()
{
    if (mTextureObject)
        return;

    auto &gl = GLContext::currentContext();
    const auto createTexture = [&]() {
        auto texture = GLuint{};
        gl.glGenTextures(1, &texture);

        if (mTextureBuffer) {
            gl.glBindTexture(mTarget, texture);
            gl.glTexBuffer(mTarget, mFormat,
                mTextureBuffer->getReadWriteBufferId());
            gl.glBindTexture(mTarget, 0);
        }
        return texture;
    };
    const auto freeTexture = [](GLuint texture) {
        auto &gl = GLContext::currentContext();
        gl.glDeleteTextures(1, &texture);
    };

    mTextureObject = GLObject(createTexture(), freeTexture);
}

void GLTexture::upload()
{
    if (!mSystemCopyModified)
        return;

    auto &gl = GLContext::currentContext();

    auto data = mData;
    if (mData.format() != mFormat)
        data = mData.convert(mFormat);

    auto textureId = static_cast<GLuint>(mTextureObject);
    if (!upload(gl, data, &textureId)) {
        mMessages += MessageList::insert(
            mItemId, MessageType::UploadingImageFailed);
        return;
    }
    mSystemCopyModified = mDeviceCopyModified = false;
}

bool GLTexture::download()
{
    if (mTextureBuffer)
        return mTextureBuffer->download(false);

    if (!mDeviceCopyModified)
        return false;

    auto &gl = GLContext::currentContext();
    if (!download(gl, mData, mTextureObject)) {
        mMessages += MessageList::insert(
            mItemId, MessageType::DownloadingImageFailed);
        return false;
    }
    mSystemCopyModified = mDeviceCopyModified = false;
    return true;
}
