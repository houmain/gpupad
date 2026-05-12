#pragma once

#include "editors/texture/TextureEditorBackground.h"
#include <memory>

class GLWindow;
class QOpenGLShaderProgram;

class GLTextureEditorBackground final : public TextureEditorBackground
{
public:
    explicit GLTextureEditorBackground(GLWindow *parent);
    ~GLTextureEditorBackground() override;

    void releaseGpu() override;
    void paintGpu(const QSizeF &size, const QPointF &offset) override;

private:
    GLWindow &mWindow;
    std::unique_ptr<QOpenGLShaderProgram> mProgram;
};
