#pragma once

#include "editors/texture/TextureEditorItem.h"
#include <memory>

class GLWindow;

class GLTextureEditorItem final : public TextureEditorItem
{
public:
    explicit GLTextureEditorItem(GLWindow *parent);
    ~GLTextureEditorItem() override;

    void releaseGpu() override;
    void paintGpu(const QMatrix4x4 &transform) override;
    bool downloadImage(TextureData *image) override;
    void setPreviewTexture(ShareSyncPtr shareSync, ShareHandle textureHandle,
        int samples) override;

private:
    class ProgramCache;

    GLWindow &window();
    void imageChanged() override;
    bool updateTexture();
    bool renderTexture(const QMatrix4x4 &transform);

    std::unique_ptr<ProgramCache> mProgramCache;
    ShareSyncPtr mShareSync;
    GLuint mImageTextureId{};
    GLuint mPreviewTextureId{};
};
