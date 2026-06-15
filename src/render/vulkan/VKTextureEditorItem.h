#pragma once
#if defined(VULKAN_ENABLED)

#  include "editors/texture/TextureEditorItem.h"
#  include "render/opengl/GLContext.h"
#  include <QOffscreenSurface>

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
    void paintGpu(const QSizeF &bounds, const QPointF &offset) override;
    void submittedGpu() override;
    bool downloadImage(TextureData *image) override;
    void copySharedTexture(ShareHandle textureHandle, int samples) override;

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

    // OpenGL path: copySharedTexture gets a producer GL texture id, exports
    // editor-owned mTexture to GL, blits the GL source into that view, then
    // transitions mTexture back for Vulkan rendering.
    struct GLState
    {
        std::unique_ptr<GLContext> context;
        std::unique_ptr<QOffscreenSurface> surface;
        ShareHandle importedShareHandle{};
        GLuint textureId{};
    };

    struct TextureBinding;
    struct PipelineCache;

    VKWindow &window();
    VKContext makeContext();
    void submitCommandQueue(VKContext &context);
    bool ensureGLContext();
    void releaseGL();
    bool uploadTexture();
    bool copyImportedTexture(ShareHandle textureHandle);
    bool importShareHandle(VKContext &context, ShareHandle shareHandle);
    void releaseShareState();
    bool copyShareStateToTexture(VKContext &context);
    bool copyOpenGLTexture(ShareHandle textureHandle);
    bool renderTexture(const QMatrix4x4 &transform);
    void resetTextureBinding();

    std::unique_ptr<PipelineCache> mPipelineCache;
    std::unique_ptr<ShareState> mShare;
    std::unique_ptr<GLState> mGl;
    std::unique_ptr<VKTexture> mTexture;
    std::unique_ptr<TextureBinding> mTextureBinding;
    ShareHandle mSharedTextureHandle{};
};

#endif // defined(VULKAN_ENABLED)
