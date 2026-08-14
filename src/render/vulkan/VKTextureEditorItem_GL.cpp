#include "VKTextureEditorItem.h"
#include "VKTexture.h"
#include "VKWindow.h"

#if defined(OPENGL_ENABLED)

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

    bool importSharedTexture(GLContext &gl, const ShareHandleData &shareHandle,
        const TextureData &data, int samples, GLuint textureId)
    {
        static auto glCreateMemoryObjectsEXT =
            gl.getProcAddress<PFNGLCREATEMEMORYOBJECTSEXTPROC>(
                "glCreateMemoryObjectsEXT");
        static auto glDeleteMemoryObjectsEXT =
            gl.getProcAddress<PFNGLDELETEMEMORYOBJECTSEXTPROC>(
                "glDeleteMemoryObjectsEXT");
        static auto glMemoryObjectParameterivEXT =
            gl.getProcAddress<PFNGLMEMORYOBJECTPARAMETERIVEXTPROC>(
                "glMemoryObjectParameterivEXT");
#  if defined(_WIN32)
        static auto glImportMemoryWin32HandleEXT =

            gl.getProcAddress<PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC>(
                "glImportMemoryWin32HandleEXT");
#  else
        static auto glImportMemoryFdEXT =
            gl.getProcAddress<PFNGLIMPORTMEMORYFDEXTPROC>(
                "glImportMemoryFdEXT");
#  endif
        static auto glTextureStorageMem1DEXT =
            gl.getProcAddress<PFNGLTEXTURESTORAGEMEM1DEXTPROC>(
                "glTextureStorageMem1DEXT");
        static auto glTextureStorageMem2DEXT =
            gl.getProcAddress<PFNGLTEXTURESTORAGEMEM2DEXTPROC>(
                "glTextureStorageMem2DEXT");
        static auto glTextureStorageMem2DMultisampleEXT =
            gl.getProcAddress<PFNGLTEXTURESTORAGEMEM2DMULTISAMPLEEXTPROC>(
                "glTextureStorageMem2DMultisampleEXT");
        static auto glTextureStorageMem3DEXT =
            gl.getProcAddress<PFNGLTEXTURESTORAGEMEM3DEXTPROC>(
                "glTextureStorageMem3DEXT");
        static auto glTextureStorageMem3DMultisampleEXT =
            gl.getProcAddress<PFNGLTEXTURESTORAGEMEM3DMULTISAMPLEEXTPROC>(
                "glTextureStorageMem3DMultisampleEXT");

        if (!glCreateMemoryObjectsEXT || !glDeleteMemoryObjectsEXT
            || !glMemoryObjectParameterivEXT
#  if defined(_WIN32)
            || !glImportMemoryWin32HandleEXT
#  else
            || !glImportMemoryFdEXT
#  endif
            || !glTextureStorageMem1DEXT || !glTextureStorageMem2DEXT
            || !glTextureStorageMem2DMultisampleEXT || !glTextureStorageMem3DEXT
            || !glTextureStorageMem3DMultisampleEXT)
            return false;

        auto memoryObject = GLuint{ };
        glCreateMemoryObjectsEXT(1, &memoryObject);
        auto dedicated = GLint{ shareHandle.dedicated ? GL_TRUE : GL_FALSE };
        glMemoryObjectParameterivEXT(memoryObject,
            GL_DEDICATED_MEMORY_OBJECT_EXT, &dedicated);
#  if defined(_WIN32)
        glImportMemoryWin32HandleEXT(memoryObject, shareHandle.allocationSize,
            static_cast<GLenum>(shareHandle.type), shareHandle.handle);
#  else
        glImportMemoryFdEXT(memoryObject, shareHandle.allocationSize,
            static_cast<GLenum>(shareHandle.type),
            reinterpret_cast<intptr_t>(shareHandle.handle));
#  endif
        const auto target = data.getTarget(samples);
        const auto dimensions = data.dimensions() + (data.isArray() ? 1 : 0);
        if (dimensions == 1) {
            glTextureStorageMem1DEXT(textureId, data.levels(),
                static_cast<GLenum>(data.format()), data.width(), memoryObject,
                shareHandle.allocationOffset);
        } else if (dimensions == 2) {
            if (isMultisampleTarget(target)) {
                glTextureStorageMem2DMultisampleEXT(textureId, samples,
                    static_cast<GLenum>(data.format()), data.width(),
                    textureHeight(data, target, 0), true, memoryObject,
                    shareHandle.allocationOffset);
            } else {
                glTextureStorageMem2DEXT(textureId, data.levels(),
                    static_cast<GLenum>(data.format()), data.width(),
                    textureHeight(data, target, 0), memoryObject,
                    shareHandle.allocationOffset);
            }
        } else if (dimensions == 3) {
            if (isMultisampleTarget(target)) {
                glTextureStorageMem3DMultisampleEXT(textureId, samples,
                    static_cast<GLenum>(data.format()), data.width(),
                    data.height(), textureDepth(data, target, 0), true,
                    memoryObject, shareHandle.allocationOffset);
            } else {
                glTextureStorageMem3DEXT(textureId, data.levels(),
                    static_cast<GLenum>(data.format()), data.width(),
                    data.height(), textureDepth(data, target, 0), memoryObject,
                    shareHandle.allocationOffset);
            }
        }
        glDeleteMemoryObjectsEXT(1, &memoryObject);
        return (glGetError() == GL_NO_ERROR);
    }
} // namespace

bool VKTextureEditorItem::GLState::initialize()
{
    context.setShareContext(QOpenGLContext::globalShareContext());
    surface.setFormat(context.format());
    surface.create();
    return (context.create() && context.makeCurrent(&surface)
        && gl.initialize(&context));
}

VKTextureEditorItem::GLState::~GLState()
{
    const auto madeCurrent = context.makeCurrent(&surface);
    Q_ASSERT(madeCurrent);
    if (madeCurrent && textureId)
        gl.glDeleteTextures(1, &textureId);
}

bool VKTextureEditorItem::copyGLTexture(VKContext &context,
    const ShareHandleData &shareHandle, const TextureData &image)
{
    if (shareHandle.type != ShareHandleType::OPENGL_TEXTURE_ID)
        return false;

    if (!mGLState) {
        auto state = std::make_unique<GLState>();
        if (!state->initialize())
            return false;
        mGLState = std::move(state);
    }
    if (!mGLState->context.makeCurrent(&mGLState->surface))
        return false;

    auto &state = *mGLState;
    auto &gl = state.gl;

    const auto sourceTextureId = static_cast<GLuint>(
        reinterpret_cast<std::uintptr_t>(shareHandle.handle));
    if (!gl.glIsTexture(sourceTextureId))
        return false;

    if (!mTexture) {
        auto cleanup = qScopeGuard([&] {
            if (state.textureId)
                gl.glDeleteTextures(1, &state.textureId);
            state.textureId = GL_NONE;
            if (mTexture)
                mTexture->release(context.device);
            mTexture.reset();
        });

        mTexture = std::make_unique<VKTexture>(image, mTextureSamples);
        mTexture->boundAsSampler();

        const auto textureHandle =
            mTexture->getExternalMemoryShareHandle(context);
        if (!textureHandle.handle)
            return false;

        gl.glCreateTextures(image.getTarget(mTextureSamples), 1,
            &state.textureId);
        if (!importSharedTexture(gl, textureHandle, image, mTextureSamples,
                state.textureId))
            return false;

        cleanup.dismiss();
    }

    // Put the Vulkan image in a preserving layout before OpenGL writes it.
    // Transitioning it from Undefined only after the GL copy discards the
    // externally written contents.
    if (!mTexture->prepareExternalWrite(context))
        return false;
    window().submitCommandQueueWaitIdle();

    if (!copyTexture(gl, image, mTextureSamples, sourceTextureId,
            state.textureId))
        return false;
    gl.glFinish();

    return true;
}

#endif // defined(OPENGL_ENABLED)
