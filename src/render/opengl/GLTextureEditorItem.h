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
    void paintGpu(const QSizeF &bounds, const QPointF &offset) override;
    bool downloadImage(TextureData *image) override;
    void copySharedTexture(ShareHandle textureHandle, int samples) override;

private:
    class ProgramCache;

    GLWindow &window();
    bool uploadTexture();
    bool renderTexture(const QMatrix4x4 &transform);

    std::unique_ptr<ProgramCache> mProgramCache;
    QOpenGLTexture mPickerTexture{ QOpenGLTexture::Target1D };
    GLuint mImageTextureId{};
    GLuint mSharedTextureId{};
};
