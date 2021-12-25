#include "GLTexture.h"
#include "GLBuffer.h"
#include "scripting/ScriptEngine.h"
#include <QOpenGLPixelTransferOptions>
#include <cmath>

GLTexture::GLTexture(const Texture &texture, ScriptEngine &scriptEngine)
    : mItemId(texture.id)
    , mFileName(texture.fileName)
    , mFlipVertically(texture.flipVertically)
    , mTarget(texture.target)
    , mFormat(texture.format)
    , mWidth(std::max(scriptEngine.evaluateInt(texture.width, mItemId, mMessages), 1))
    , mHeight(std::max(scriptEngine.evaluateInt(texture.height, mItemId, mMessages), 1))
    , mDepth(std::max(scriptEngine.evaluateInt(texture.depth, mItemId, mMessages), 1))
    , mLayers(std::max(scriptEngine.evaluateInt(texture.layers, mItemId, mMessages), 1))
    , mSamples(texture.samples)
    , mKind(getKind(texture))
{
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
    auto fbo = createFramebuffer(getReadWriteTextureId(), 0);
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

        const auto dataType = getTextureDataType(mFormat);
        switch (dataType) {
            case TextureDataType::Normalized_sRGB:
            case TextureDataType::Float:
                for (auto i = 0u; i < 3; ++i)
                    color[i] = srgbToLinear(color[i]);
                break;
            case TextureDataType::Int8: multiplyRGBA(0x7F); break;
            case TextureDataType::Int16: multiplyRGBA(0x7FFF); break;
            case TextureDataType::Int32: multiplyRGBA(0x7FFFFFFF); break;
            case TextureDataType::Uint8: multiplyRGBA(0xFF); break;
            case TextureDataType::Uint16: multiplyRGBA(0xFFFF); break;
            case TextureDataType::Uint32: multiplyRGBA(0xFFFFFFFF); break;
            case TextureDataType::Uint_10_10_10_2:
                for (auto i = 0u; i < 3; ++i)
                    color[i] *= 1024.0;
                color[3] *= 4.0;
                break;
            default:
                break;
        }

        switch (dataType) {
            case TextureDataType::Normalized:
            case TextureDataType::Normalized_sRGB:
            case TextureDataType::Float: {
                const auto values = std::array<float, 4>{
                    static_cast<float>(color[0]),
                    static_cast<float>(color[1]),
                    static_cast<float>(color[2]),
                    static_cast<float>(color[3])
                };
                gl.glClearBufferfv(GL_COLOR, 0, values.data());
                break;
            }

            case TextureDataType::Uint8:
            case TextureDataType::Uint16:
            case TextureDataType::Uint32:
            case TextureDataType::Uint_10_10_10_2: {
                const auto values = std::array<GLuint, 4>{
                    static_cast<GLuint>(color[0]),
                    static_cast<GLuint>(color[1]),
                    static_cast<GLuint>(color[2]),
                    static_cast<GLuint>(color[3])
                };
                gl.glClearBufferuiv(GL_COLOR, 0, values.data());
                break;
            }

            case TextureDataType::Int8:
            case TextureDataType::Int16:
            case TextureDataType::Int32: {
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
    return copyTexture(source.getReadOnlyTextureId(),
        getReadWriteTextureId(), 0);
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
    if (!FileDialog::isEmptyOrUntitled(mFileName))
        if (!Singletons::fileCache().getTexture(mFileName, mFlipVertically, &fileData))
            mMessages += MessageList::insert(mItemId,
                MessageType::LoadingFileFailed, mFileName);

    // reload file as long as targets match
    // and when when writing to it also dimensions match (format is ignored)
    const auto useFileData = [&]() {
        if (fileData.isNull() ||
            mTarget != fileData.target())
            return false;
        if (!forWriting)
            return true;
        return (mWidth == fileData.width() &&
                mHeight == fileData.height() &&
                mDepth == fileData.depth() &&
                mLayers == fileData.layers());
    };
    if (useFileData()) {
        mSystemCopyModified |= !mData.isSharedWith(fileData);
        mData = fileData;
    }
    else if (mData.isNull()) {
        if (!mData.create(mTarget, mFormat, mWidth, mHeight, mDepth, mLayers, mSamples)) {
            mData.create(mTarget, Texture::Format::RGBA8_UNorm, 1, 1, 1, 1, 1);
            mMessages += MessageList::insert(mItemId,
                MessageType::CreatingTextureFailed);
        }
        mData.clear();
        mSystemCopyModified |= true;
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

    if (!mData.upload(mTextureObject, mFormat)) {
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
    if (mKind.depth && mKind.stencil)
        attachment = GL_DEPTH_STENCIL_ATTACHMENT;
    else if (mKind.depth)
        attachment = GL_DEPTH_ATTACHMENT;
    else if (mKind.stencil)
        attachment = GL_STENCIL_ATTACHMENT;

    auto fbo = GLObject(createFBO(), freeFBO);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl.glFramebufferTexture(GL_FRAMEBUFFER, attachment, textureId, level);
    auto status = gl.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        fbo.reset();
    return fbo;
}

bool GLTexture::copyTexture(GLuint sourceTextureId,
    GLuint destTextureId, int level)
{
    auto &gl = GLContext::currentContext();
    const auto sourceFbo = createFramebuffer(sourceTextureId, level);
    const auto destFbo = createFramebuffer(destTextureId, level);
    if (!sourceFbo || !destFbo)
        return false;

    const auto width = mData.getLevelWidth(level);
    const auto height = mData.getLevelHeight(level);
    auto blitMask = GLbitfield{ GL_COLOR_BUFFER_BIT };
    if (mKind.depth && mKind.stencil)
        blitMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    else if (mKind.depth)
        blitMask = GL_DEPTH_BUFFER_BIT;
    else if (mKind.stencil)
        blitMask = GL_STENCIL_BUFFER_BIT;

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFbo);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFbo);
    gl.glBlitFramebuffer(0, 0, width, height, 0, 0,
        width, height, blitMask, GL_NEAREST);
    return true;
}
