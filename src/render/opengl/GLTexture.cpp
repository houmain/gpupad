#include "GLTexture.h"
#include "GLBuffer.h"
#include <QOpenGLPixelTransferOptions>
#include <QScopeGuard>
#include <cmath>

namespace {
    GLuint createFramebuffer(QOpenGLFunctions_3_3_Core &gl, GLenum target,
        GLuint textureId, GLenum attachment)
    {
        auto fbo = GLuint{};
        gl.glGenFramebuffers(1, &fbo);
        gl.glBindFramebuffer(target, fbo);
        gl.glFramebufferTexture(target, attachment, textureId, 0);
        return fbo;
    }

    bool resolveTexture(QOpenGLFunctions_3_3_Core &gl, GLuint sourceTextureId,
        GLuint destTextureId, int width, int height,
        QOpenGLTexture::TextureFormat format)
    {
        auto blitMask = GLbitfield{};
        auto attachment = GLenum{};
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

        auto previousTarget = GLint{};
        gl.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousTarget);
        const auto sourceFbo = createFramebuffer(gl, GL_READ_FRAMEBUFFER,
            sourceTextureId, attachment);
        const auto destFbo = createFramebuffer(gl, GL_DRAW_FRAMEBUFFER,
            destTextureId, attachment);
        gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, blitMask,
            GL_NEAREST);
        gl.glDeleteFramebuffers(1, &sourceFbo);
        gl.glDeleteFramebuffers(1, &destFbo);
        gl.glBindFramebuffer(GL_FRAMEBUFFER, previousTarget);
        return (glGetError() == GL_NONE);
    }

    bool uploadMultisample(QOpenGLFunctions_3_3_Core &gl,
        const TextureData &data, QOpenGLTexture::Target target, int samples,
        GLuint textureId)
    {
        gl.glBindTexture(target, textureId);
        if (target == QOpenGLTexture::Target2DMultisample) {
            gl.glTexImage2DMultisample(target, samples, data.format(),
                data.width(), data.height(), GL_FALSE);

            // upload single sample and resolve
            auto singleSampleTextureId = GLuint{};
            const auto cleanup = qScopeGuard(
                [&] { gl.glDeleteTextures(1, &singleSampleTextureId); });
            return (data.uploadGL(&singleSampleTextureId)
                && resolveTexture(gl, singleSampleTextureId, textureId,
                    data.width(), data.height(), data.format()));
        } else {
            Q_ASSERT(target == QOpenGLTexture::Target2DMultisampleArray);
            gl.glTexImage3DMultisample(target, samples, data.format(),
                data.width(), data.height(), data.layers(), GL_FALSE);

            // TODO: upload
        }
        Q_ASSERT(!"not implemented");
        return false;
    }

    bool download(QOpenGLFunctions_3_3_Core &gl, TextureData &data,
        QOpenGLTexture::Target target, GLuint textureId)
    {
        gl.glBindTexture(target, textureId);
        for (auto level = 0; level < data.levels(); ++level) {
            if (data.isCompressed()) {
                auto size = GLint{};
                gl.glGetTexLevelParameteriv(target, level,
                    GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
                if (glGetError() != GL_NO_ERROR
                    || size > data.getImageSize(level))
                    return false;
                gl.glGetCompressedTexImage(target, level,
                    data.getWriteonlyData(level, 0, 0));
            } else {
                gl.glGetTexImage(target, level, data.pixelFormat(),
                    data.pixelType(), data.getWriteonlyData(level, 0, 0));
            }
        }
        return (glGetError() == GL_NO_ERROR);
    }

    bool downloadCubemap(QOpenGLFunctions_3_3_Core &gl, TextureData &data,
        QOpenGLTexture::Target target, GLuint textureId)
    {
        // TODO: download
        Q_ASSERT(!"not implemented");
        return true;
    }

    bool downloadMultisample(QOpenGLFunctions_3_3_Core &gl, TextureData &data,
        QOpenGLTexture::Target target, GLuint textureId)
    {
        if (target == QOpenGLTexture::Target2DMultisample) {
            // create single sample texture (=upload), resolve, download
            auto singleSampleTextureId = GLuint{};
            const auto cleanup = qScopeGuard(
                [&] { gl.glDeleteTextures(1, &singleSampleTextureId); });
            if (!data.uploadGL(&singleSampleTextureId)
                || !resolveTexture(gl, textureId, singleSampleTextureId,
                    data.width(), data.height(), data.format())
                || !download(gl, data, QOpenGLTexture::Target2D,
                    singleSampleTextureId))
                return false;

            return true;
        } else {
            Q_ASSERT(target == QOpenGLTexture::Target2DMultisampleArray);
            // TODO: download
        }
        Q_ASSERT(!"not implemented");
        return false;
    }

    GLObject createFramebuffer(QOpenGLFunctions_3_3_Core &gl,
        const TextureKind &kind, GLuint textureId, int level)
    {
        const auto createFBO = [&]() {
            auto fbo = GLuint{};
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

    bool copyTexture(QOpenGLFunctions_3_3_Core &gl, const TextureKind &kind,
        GLuint sourceTextureId, GLuint destTextureId, int width, int height,
        int level)
    {
        const auto sourceFbo =
            createFramebuffer(gl, kind, sourceTextureId, level);
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
        gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, blitMask,
            GL_NEAREST);
        return true;
    }
} // namespace

bool GLTexture::upload(QOpenGLFunctions_3_3_Core &gl, const TextureData &data,
    QOpenGLTexture::Target target, int samples, GLuint *textureId)
{
    Q_ASSERT(target && textureId);
    if (data.isNull())
        return false;

    auto cleanup = qScopeGuard([&] {
        gl.glDeleteTextures(1, textureId);
        *textureId = GL_NONE;
    });

    if (!*textureId) {
        gl.glGenTextures(1, textureId);
    } else {
        cleanup.dismiss();
    }

    if (isMultisampleTarget(target)) {
        if (!uploadMultisample(gl, data, target, samples, *textureId))
            return false;
    } else {
        if (!data.uploadGL(textureId))
            return false;
    }
    cleanup.dismiss();
    return true;
}

bool GLTexture::download(QOpenGLFunctions_3_3_Core &gl, TextureData &data,
    QOpenGLTexture::Target target, GLuint textureId)
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);

    if (isMultisampleTarget(target))
        return downloadMultisample(gl, data, target, textureId);

    if (isCubemapTarget(target))
        return downloadCubemap(gl, data, target, textureId);

    return ::download(gl, data, target, textureId);
}

GLTexture::GLTexture(const Texture &texture, ScriptEngine &scriptEngine)
    : TextureBase(texture, scriptEngine)
{
}

GLTexture::GLTexture(const Buffer &buffer, GLBuffer *textureBuffer,
    Texture::Format format, ScriptEngine &scriptEngine)
    : TextureBase(buffer, format, scriptEngine)
    , mTextureBuffer(textureBuffer)
{
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
        gl.glClearBufferfi(GL_DEPTH_STENCIL, 0, static_cast<float>(depth),
            stencil);
    } else if (mKind.depth) {
        const auto d = static_cast<float>(depth);
        gl.glClearBufferfv(GL_DEPTH, 0, &d);
    } else if (mKind.stencil) {
        gl.glClearBufferiv(GL_STENCIL, 0, &stencil);
    } else {
        const auto sampleType = getTextureSampleType(mFormat);
        transformClearColor(color, sampleType);

        switch (sampleType) {
        case TextureSampleType::Normalized:
        case TextureSampleType::Normalized_sRGB:
        case TextureSampleType::Float:           {
            const auto values = std::array<float, 4>{
                static_cast<float>(color[0]), static_cast<float>(color[1]),
                static_cast<float>(color[2]), static_cast<float>(color[3])
            };
            gl.glClearBufferfv(GL_COLOR, 0, values.data());
            break;
        }

        case TextureSampleType::Uint8:
        case TextureSampleType::Uint16:
        case TextureSampleType::Uint32:
        case TextureSampleType::Uint_10_10_10_2: {
            const auto values = std::array<GLuint, 4>{
                static_cast<GLuint>(color[0]), static_cast<GLuint>(color[1]),
                static_cast<GLuint>(color[2]), static_cast<GLuint>(color[3])
            };
            gl.glClearBufferuiv(GL_COLOR, 0, values.data());
            break;
        }

        case TextureSampleType::Int8:
        case TextureSampleType::Int16:
        case TextureSampleType::Int32: {
            const auto values = std::array<GLint, 4>{
                static_cast<GLint>(color[0]), static_cast<GLint>(color[1]),
                static_cast<GLint>(color[2]), static_cast<GLint>(color[3])
            };
            gl.glClearBufferiv(GL_COLOR, 0, values.data());
            break;
        }

        default: succeeded = false; break;
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
        getReadWriteTextureId(), mData.getLevelWidth(level),
        mData.getLevelHeight(level), level);
}

bool GLTexture::swap(GLTexture &other)
{
    if (!TextureBase::swap(other))
        return false;

    std::swap(mTextureBuffer, other.mTextureBuffer);
    std::swap(mTextureObject, other.mTextureObject);
    return true;
}

bool GLTexture::updateMipmaps()
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);
    if (mMipmapsInvalidated) {
        if (levels() > 1) {
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

    TextureBase::reload(forWriting);

    // set multisample target
    mTarget = mData.getTarget(mSamples);
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
    if (!upload(gl, data, mTarget, mSamples, &textureId)) {
        mMessages += MessageList::insert(mItemId,
            (data.isNull() ? MessageType::UploadingImageFailed
                           : MessageType::CreatingTextureFailed));
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
    if (!download(gl, mData, mTarget, mTextureObject)) {
        mMessages +=
            MessageList::insert(mItemId, MessageType::DownloadingImageFailed);
        return false;
    }
    mSystemCopyModified = mDeviceCopyModified = false;
    return true;
}
