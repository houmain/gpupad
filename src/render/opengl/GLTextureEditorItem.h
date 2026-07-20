#pragma once

#include "editors/texture/TextureEditorItem.h"
#include <QOpenGLTexture>
#include <memory>

class GLWindow;

class GLTextureEditorItem final : public TextureEditorItem
{
public:
    explicit GLTextureEditorItem(GLWindow *parent);
    ~GLTextureEditorItem() override;

    void releaseGpu() override;
    void paintGpu(const QSizeF &bounds, const QPointF &offset,
        const TextureData &image) override;
    bool downloadImage(TextureData *image) override;
    bool copySharedTexture(ShareHandle shareHandle, int samples,
        const TextureData &image) override;

private:
    class ProgramCache;

    GLWindow &window();
    bool uploadImage(const TextureData &image) override;
    bool renderTexture(const QMatrix4x4 &transform, const TextureData &image);

    std::unique_ptr<ProgramCache> mProgramCache;
    QOpenGLTexture mPickerTexture{ QOpenGLTexture::Target1D };
    GLuint mImageTextureId{};
    GLuint mSharedTextureId{};
};
