#pragma once
#if defined(VULKAN_ENABLED)

#  include "editors/texture/TextureEditorItem.h"
#  if defined(OPENGL_ENABLED)
#    include "render/opengl/GLContext.h"
#    include <QOffscreenSurface>
#  endif

class VKWindow;
class VKTexture;
struct VKContext;

class VKTextureEditorItem final : public TextureEditorItem
{
public:
    explicit VKTextureEditorItem(VKWindow *parent);
    ~VKTextureEditorItem() override;

    void releaseGpu() override;
    void prepareGpu() override;
    void paintGpu(const QSizeF &bounds, const QPointF &offset,
        const TextureData &image) override;
    void submittedGpu() override;
    bool downloadImage(TextureData *image) override;
    bool copySharedTexture(ShareHandle textureHandle, int samples,
        const TextureData &image) override;

private:
    // Shared-memory path: copySharedTexture gets a producer memory handle,
    // imports it as a source VkImage/VKTexture, then copies that source into
    // the editor-owned mTexture used for rendering.
    struct ShareState
    {
        ShareHandle shareHandle{};
        VkImage image{};
        VkDeviceMemory memory{};
        std::unique_ptr<VKTexture> texture;
    };

#  if defined(OPENGL_ENABLED)
    // OpenGL path: copySharedTexture gets a producer GL texture id, exports
    // editor-owned mTexture to GL, blits the GL source into that view, then
    // transitions mTexture back for Vulkan rendering.
    struct GLState
    {
        QOpenGLContext context;
        QOffscreenSurface surface;
        GLContext gl;
        ShareHandle importedShareHandle{};
        GLuint textureId{};
    };
#  endif

    struct TextureBinding;
    struct PipelineCache;

    VKWindow &window();
    VKContext makeContext();
    void submitCommandQueue(VKContext &context);
    bool uploadImage(const TextureData &image) override;
    bool copyVKTexture(VKTexture &source);
    bool copyImportedTexture(ShareHandle textureHandle,
        const TextureData &image);
    bool importShareHandle(VKContext &context, ShareHandle shareHandle,
        const TextureData &image);
    void releaseShareState();
    bool copyShareStateToTexture(VKContext &context, const TextureData &image);
    bool renderTexture(const QMatrix4x4 &transform, const TextureData &image);
    void resetTextureBinding();

#  if defined(OPENGL_ENABLED)
    bool makeGLContextCurrent();
    void releaseGL();
    bool copyGLTexture(ShareHandle textureHandle, const TextureData &image);
#  endif

    std::unique_ptr<PipelineCache> mPipelineCache;
    std::unique_ptr<ShareState> mShare;
#  if defined(OPENGL_ENABLED)
    std::unique_ptr<GLState> mGLState;
#  endif
    std::unique_ptr<VKTexture> mTexture;
    std::unique_ptr<TextureBinding> mTextureBinding;
    ShareHandle mSharedTextureHandle{};
};

#endif // defined(VULKAN_ENABLED)
