#pragma once
#if defined(VULKAN_ENABLED)

#  include "VKWindow.h"
#  include "editors/texture/TextureEditorItem.h"
#  if defined(OPENGL_ENABLED)
#    include "render/opengl/GLContext.h"
#    include <QOffscreenSurface>
#  endif

class VKTexture;

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
    bool uploadImage(const TextureData &image) override;
    bool downloadImage(TextureData *image) override;
    bool copySharedTexture(ShareHandle shareHandle, int samples,
        const TextureData &image) override;

private:
    // Vulkan texture pointer sharing path
    bool copyVKTexture(VKContext &context, VKTexture &source);

    // Shared-memory path: copySharedTexture gets a producer memory handle,
    // imports it as a source VkImage/VKTexture, then copies that source into
    // the editor-owned mTexture used for rendering.
    struct ShareState
    {
        ~ShareState();
        bool initialize(VKContext &context, const ShareHandleData &shareHandle,
            const TextureData &image, int samples);
        void release(KDGpu::Device &device);

        VkImage vkImage{};
        VkDeviceMemory vkMemory{};
        std::unique_ptr<VKTexture> vkTexture;
    };
    bool copySharedTexture(VKContext &context,
        const ShareHandleData &shareHandle, const TextureData &image);
    std::unique_ptr<ShareState> mShare;

#  if defined(OPENGL_ENABLED)
    // OpenGL path: copySharedTexture gets a producer GL texture id, exports
    // editor-owned mTexture to GL, blits the GL source into that view, then
    // transitions mTexture back for Vulkan rendering.
    struct GLState
    {
        ~GLState();
        bool initialize();

        QOpenGLContext context;
        QOffscreenSurface surface;
        GLContext gl;
        GLuint textureId{};
    };
    bool copyGLTexture(VKContext &context, const ShareHandleData &shareHandle,
        const TextureData &image);
    std::unique_ptr<GLState> mGLState;
#  endif

    struct PipelineCache;
    struct TextureBinding;

    VKWindow &window();
    void releaseTextureSharing(VKContext &context);
    bool renderTexture(const QMatrix4x4 &transform, const TextureData &image);

    std::unique_ptr<PipelineCache> mPipelineCache;
    std::unique_ptr<TextureBinding> mTextureBinding;
    std::unique_ptr<VKTexture> mTexture;
    ShareHandle mCurrentShareHandle{};
};

#endif // defined(VULKAN_ENABLED)
