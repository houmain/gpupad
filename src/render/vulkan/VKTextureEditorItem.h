#pragma once

#include "editors/texture/TextureEditorItem.h"
#include <memory>

class VKWindow;
class VKTexture;
struct VKContext;

class VKTextureEditorItem final : public TextureEditorItem
{
public:
    explicit VKTextureEditorItem(VKWindow *parent);
    ~VKTextureEditorItem() override;

    void releaseGpu() override;
    void paintGpu(const QMatrix4x4 &transform) override;
    bool downloadImage(TextureData *image) override;
    void setPreviewTexture(ShareSyncPtr shareSync, ShareHandle textureHandle,
        int samples) override;

private:
    struct GLState;
    struct GLStateDeleter
    {
        void operator()(GLState *state) const;
    };
    struct ShareState;
    struct ShareStateDeleter
    {
        void operator()(ShareState *state) const;
    };
    struct TextureBinding;
    struct PipelineCache;

    VKWindow &window();
    VKContext makeContext();
    void submitCommandQueue(VKContext &context);
    bool ensureGLContext();
    void releaseGL();
    bool updateTexture();
    bool updateImportedTexture();
    bool importShareHandle(VKContext &context, ShareHandle shareHandle);
    bool updateOpenGLTexture();
    bool renderTexture(const QMatrix4x4 &transform);
    void resetTextureBinding();

    std::unique_ptr<PipelineCache> mPipelineCache;
    std::unique_ptr<ShareState, ShareStateDeleter> mShare;
    std::unique_ptr<GLState, GLStateDeleter> mGl;
    std::unique_ptr<VKTexture> mTexture;
    std::unique_ptr<TextureBinding> mTextureBinding;
    ShareSyncPtr mShareSync;
    ShareHandle mPreviewTextureHandle{};
};
