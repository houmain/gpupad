#include "VKTextureEditorItem.h"
#include "VKTexture.h"
#include "VKWindow.h"

namespace {
    int textureLevelCount(const TextureData &data, int samples)
    {
        return (samples > 1 ? 1 : data.levels());
    }

    int textureHeight(const TextureData &data, Texture::Target target,
        int level)
    {
        if (target == Texture::Target::Target1DArray)
            return data.layers();
        return data.getLevelHeight(level);
    }

    int textureDepth(const TextureData &data, Texture::Target target, int level)
    {
        switch (target) {
        case Texture::Target::Target2DArray:
        case Texture::Target::Target2DMultisampleArray: return data.layers();
        case Texture::Target::Target3D:                 return data.getLevelDepth(level);
        case Texture::Target::TargetCubeMap:
        case Texture::Target::TargetCubeMapArray:
            return data.layers() * data.faces();
        default: return 1;
        }
    }

    bool copyTexture(GLContext &gl, const TextureData &data, int samples,
        GLuint sourceTextureId, GLuint destTextureId)
    {
        const auto textureTarget = data.getTarget(samples);
        const auto glTarget = static_cast<GLenum>(textureTarget);
        for (auto level = 0; level < textureLevelCount(data, samples);
            ++level) {
            gl.glCopyImageSubData(sourceTextureId, glTarget, level, 0, 0, 0,
                destTextureId, glTarget, level, 0, 0, 0,
                data.getLevelWidth(level),
                textureHeight(data, textureTarget, level),
                textureDepth(data, textureTarget, level));
        }
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
                    textureHeight(data, target, 0), true, memoryObject,
                    handle.allocationOffset);
            } else {
                glTextureStorageMem2DEXT(textureId, data.levels(),
                    static_cast<GLenum>(data.format()), data.width(),
                    textureHeight(data, target, 0), memoryObject,
                    handle.allocationOffset);
            }
        } else if (dimensions == 3) {
            if (isMultisampleTarget(target)) {
                glTextureStorageMem3DMultisampleEXT(textureId, samples,
                    static_cast<GLenum>(data.format()), data.width(),
                    data.height(), textureDepth(data, target, 0), true,
                    memoryObject, handle.allocationOffset);
            } else {
                glTextureStorageMem3DEXT(textureId, data.levels(),
                    static_cast<GLenum>(data.format()), data.width(),
                    data.height(), textureDepth(data, target, 0), memoryObject,
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
    return state.context->initializeCurrentContext(true);
}

void VKTextureEditorItem::releaseGL()
{
    if (!mGl)
        return;

    auto &state = *mGl;
    if (state.context && state.surface
        && state.context->makeCurrent(state.surface.get())) {
        if (state.textureId)
            state.context->glDeleteTextures(1, &state.textureId);
        state.context->shutdownCurrentContext();
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

    if (!copyTexture(gl, mImage, mTextureSamples, sourceTextureId,
            state.textureId))
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
