#include "VKTextureEditorItem.h"
#include "VKTexture.h"
#include "VKWindow.h"

namespace {
    GLenum attachmentForFormat(Texture::Format format)
    {
        switch (format) {
        default: return GL_COLOR_ATTACHMENT0;

        case Texture::Format::D16:
        case Texture::Format::D24:
        case Texture::Format::D32:
        case Texture::Format::D32F: return GL_DEPTH_ATTACHMENT;

        case Texture::Format::S8: return GL_STENCIL_ATTACHMENT;

        case Texture::Format::D24S8:
        case Texture::Format::D32FS8X24: return GL_DEPTH_STENCIL_ATTACHMENT;
        }
    }

    GLbitfield blitMaskForFormat(Texture::Format format)
    {
        switch (format) {
        default: return GL_COLOR_BUFFER_BIT;

        case Texture::Format::D16:
        case Texture::Format::D24:
        case Texture::Format::D32:
        case Texture::Format::D32F: return GL_DEPTH_BUFFER_BIT;

        case Texture::Format::S8: return GL_STENCIL_BUFFER_BIT;

        case Texture::Format::D24S8:
        case Texture::Format::D32FS8X24:
            return GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
        }
    }

    GLuint createFramebuffer(GLContext &gl, GLenum target, GLuint textureId,
        GLenum attachment)
    {
        auto fbo = GLuint{};
        gl.glGenFramebuffers(1, &fbo);
        gl.glBindFramebuffer(target, fbo);
        gl.glFramebufferTexture(target, attachment, textureId, 0);
        return fbo;
    }

    bool copyTexture(GLContext &gl, const TextureData &data,
        GLuint sourceTextureId, GLuint destTextureId)
    {
        const auto attachment = attachmentForFormat(data.format());
        auto previousTarget = GLint{};
        gl.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousTarget);

        const auto sourceFbo = createFramebuffer(gl, GL_READ_FRAMEBUFFER,
            sourceTextureId, attachment);
        const auto destFbo = createFramebuffer(gl, GL_DRAW_FRAMEBUFFER,
            destTextureId, attachment);
        gl.glBlitFramebuffer(0, 0, data.width(), data.height(), 0, 0,
            data.width(), data.height(), blitMaskForFormat(data.format()),
            GL_NEAREST);
        gl.glDeleteFramebuffers(1, &sourceFbo);
        gl.glDeleteFramebuffers(1, &destFbo);
        gl.glBindFramebuffer(GL_FRAMEBUFFER, previousTarget);
        return (glGetError() == GL_NO_ERROR);
    }

    bool importSharedTexture(GLContext &gl, ShareHandle handle,
        const TextureData &data, int samples, GLuint textureId)
    {
        if (!handle)
            return false;

        static auto glCreateMemoryObjectsEXT =
            reinterpret_cast<PFNGLCREATEMEMORYOBJECTSEXTPROC>(
                gl.getProcAddress("glCreateMemoryObjectsEXT"));
        static auto glDeleteMemoryObjectsEXT =
            reinterpret_cast<PFNGLDELETEMEMORYOBJECTSEXTPROC>(
                gl.getProcAddress("glDeleteMemoryObjectsEXT"));
        static auto glMemoryObjectParameterivEXT =
            reinterpret_cast<PFNGLMEMORYOBJECTPARAMETERIVEXTPROC>(
                gl.getProcAddress("glMemoryObjectParameterivEXT"));
#if defined(_WIN32)
        static auto glImportMemoryWin32HandleEXT =
            reinterpret_cast<PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC>(
                gl.getProcAddress("glImportMemoryWin32HandleEXT"));
#else
        static auto glImportMemoryFdEXT =
            reinterpret_cast<PFNGLIMPORTMEMORYFDEXTPROC>(
                gl.getProcAddress("glImportMemoryFdEXT"));
#endif
        static auto glTextureStorageMem1DEXT =
            reinterpret_cast<PFNGLTEXTURESTORAGEMEM1DEXTPROC>(
                gl.getProcAddress("glTextureStorageMem1DEXT"));
        static auto glTextureStorageMem2DEXT =
            reinterpret_cast<PFNGLTEXTURESTORAGEMEM2DEXTPROC>(
                gl.getProcAddress("glTextureStorageMem2DEXT"));
        static auto glTextureStorageMem2DMultisampleEXT =
            reinterpret_cast<PFNGLTEXTURESTORAGEMEM2DMULTISAMPLEEXTPROC>(
                gl.getProcAddress("glTextureStorageMem2DMultisampleEXT"));
        static auto glTextureStorageMem3DEXT =
            reinterpret_cast<PFNGLTEXTURESTORAGEMEM3DEXTPROC>(
                gl.getProcAddress("glTextureStorageMem3DEXT"));
        static auto glTextureStorageMem3DMultisampleEXT =
            reinterpret_cast<PFNGLTEXTURESTORAGEMEM3DMULTISAMPLEEXTPROC>(
                gl.getProcAddress("glTextureStorageMem3DMultisampleEXT"));

        if (!glCreateMemoryObjectsEXT || !glDeleteMemoryObjectsEXT
            || !glMemoryObjectParameterivEXT
#if defined(_WIN32)
            || !glImportMemoryWin32HandleEXT
#else
            || !glImportMemoryFdEXT
#endif
            || !glTextureStorageMem1DEXT || !glTextureStorageMem2DEXT
            || !glTextureStorageMem2DMultisampleEXT || !glTextureStorageMem3DEXT
            || !glTextureStorageMem3DMultisampleEXT)
            return false;

        auto memoryObject = GLuint{};
        glCreateMemoryObjectsEXT(1, &memoryObject);
        auto dedicated = GLint{ handle.dedicated ? GL_TRUE : GL_FALSE };
        glMemoryObjectParameterivEXT(memoryObject,
            GL_DEDICATED_MEMORY_OBJECT_EXT, &dedicated);
#if defined(_WIN32)
        glImportMemoryWin32HandleEXT(memoryObject, handle.allocationSize,
            static_cast<GLenum>(handle.type), handle.handle);
#else
        glImportMemoryFdEXT(memoryObject, handle.allocationSize,
            static_cast<GLenum>(handle.type),
            reinterpret_cast<intptr_t>(handle.handle));
#endif
        const auto target = data.getTarget(samples);
        const auto dimensions = data.dimensions() + (data.isArray() ? 1 : 0);
        if (dimensions == 1) {
            glTextureStorageMem1DEXT(textureId, data.levels(),
                static_cast<GLenum>(data.format()), data.width(), memoryObject,
                handle.allocationOffset);
        } else if (dimensions == 2) {
            if (isMultisampleTarget(target)) {
                glTextureStorageMem2DMultisampleEXT(textureId, samples,
                    static_cast<GLenum>(data.format()), data.width(),
                    data.height(), true, memoryObject, handle.allocationOffset);
            } else {
                glTextureStorageMem2DEXT(textureId, data.levels(),
                    static_cast<GLenum>(data.format()), data.width(),
                    data.height(), memoryObject, handle.allocationOffset);
            }
        } else if (dimensions == 3) {
            if (isMultisampleTarget(target)) {
                glTextureStorageMem3DMultisampleEXT(textureId, samples,
                    static_cast<GLenum>(data.format()), data.width(),
                    data.height(), data.depth(), true, memoryObject,
                    handle.allocationOffset);
            } else {
                glTextureStorageMem3DEXT(textureId, data.levels(),
                    static_cast<GLenum>(data.format()), data.width(),
                    data.height(), data.depth(), memoryObject,
                    handle.allocationOffset);
            }
        }
        glDeleteMemoryObjectsEXT(1, &memoryObject);
        return (glGetError() == GL_NO_ERROR);
    }
} // namespace

bool VKTextureEditorItem::ensureGLContext()
{
    if (!mGl)
        mGl.reset(new GLState());

    auto &state = *mGl;
    if (!state.context) {
        state.context = std::make_unique<GLContext>();
        state.context->setShareContext(QOpenGLContext::globalShareContext());
        state.surface = std::make_unique<QOffscreenSurface>();
        state.surface->setFormat(state.context->format());
        state.surface->create();
        if (!state.context->create()) {
            mGl.reset();
            return false;
        }
    }
    if (!state.context->makeCurrent(state.surface.get()))
        return false;
    return state.context->initializeOpenGLFunctions();
}

void VKTextureEditorItem::releaseGL()
{
    if (!mGl)
        return;

    auto &state = *mGl;
    if (state.textureId && state.context && state.surface
        && state.context->makeCurrent(state.surface.get())) {
        state.context->glDeleteTextures(1, &state.textureId);
        state.context->doneCurrent();
    }
    mGl.reset();
}

bool VKTextureEditorItem::copyOpenGLTexture(ShareHandle textureHandle)
{
    if (textureHandle.type != ShareHandleType::OPENGL_TEXTURE_ID
        || mImage.isNull())
        return false;

    const auto sourceTextureId = static_cast<GLuint>(
        reinterpret_cast<std::uintptr_t>(textureHandle.handle));

    if (!mTexture || mTexture->samples() != mTextureSamples) {
        releaseGL();
        if (mTexture)
            mTexture->release(window().device());
        resetTextureBinding();
        mTexture = std::make_unique<VKTexture>(mImage, mTextureSamples);
        mTexture->boundAsSampler();
        mTexture->addUsage(KDGpu::TextureUsageFlagBits::ColorAttachmentBit);

        auto context = makeContext();
        context.commandRecorder =
            context.device.createCommandRecorder({ .queue = context.queue });
        if (!mTexture->prepareSampledImage(context)) {
            mTexture->release(window().device());
            mTexture.reset();
            return false;
        }
        submitCommandQueue(context);
    }

    if (!ensureGLContext())
        return false;

    auto &state = *mGl;
    auto &gl = *state.context;
    Q_ASSERT(gl.glIsTexture(sourceTextureId));

    const auto destHandle = mTexture->getShareHandle();
    if (state.importedShareHandle != destHandle) {
        if (state.textureId)
            gl.glDeleteTextures(1, &state.textureId);
        state.textureId = GL_NONE;
        state.importedShareHandle = {};

        gl.glCreateTextures(mImage.getTarget(mTextureSamples), 1,
            &state.textureId);
        auto cleanup = qScopeGuard([&] {
            if (state.textureId)
                gl.glDeleteTextures(1, &state.textureId);
            state.textureId = GL_NONE;
        });
        if (!importSharedTexture(gl, destHandle, mImage, mTextureSamples,
                state.textureId))
            return false;
        state.importedShareHandle = destHandle;
        cleanup.dismiss();
    }

    if (!copyTexture(gl, mImage, sourceTextureId, state.textureId))
        return false;
    gl.glFinish();

    auto context = makeContext();
    context.commandRecorder =
        context.device.createCommandRecorder({ .queue = context.queue });
    if (!mTexture->prepareSampledImage(context)) {
        mTexture->release(window().device());
        mTexture.reset();
        return false;
    }
    submitCommandQueue(context);
    return (mTexture && mTexture->texture().isValid());
}
