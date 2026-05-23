#include "GLTextureEditorBackground.h"
#include "GLWindow.h"
#include <QOpenGLShaderProgram>

GLTextureEditorBackground::GLTextureEditorBackground(GLWindow *parent)
    : TextureEditorBackground(parent)
    , mWindow(*parent)
{
}

GLTextureEditorBackground::~GLTextureEditorBackground() = default;

void GLTextureEditorBackground::releaseGpu()
{
    mProgram.reset();
}

void GLTextureEditorBackground::paintGpu(const QSizeF &size,
    const QPointF &offset)
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);
    auto &gl = mWindow.context();

    if (!mProgram) {
        mProgram = std::make_unique<QOpenGLShaderProgram>();
        auto &program = *mProgram;

        program.create();
        auto vertexShader = new QOpenGLShader(QOpenGLShader::Vertex, &program);
        auto fragmentShader =
            new QOpenGLShader(QOpenGLShader::Fragment, &program);
        vertexShader->compileSourceCode(vertexShaderSource);
        fragmentShader->compileSourceCode(fragmentShaderSource);
        program.addShader(vertexShader);
        program.addShader(fragmentShader);
        program.link();
    }

    const auto params = getParams(size, offset);
    const auto setColor = [&](const char *name, const Color &color) {
        mProgram->setUniformValue(name, color.r, color.g, color.b, color.a);
    };

    mProgram->bind();
    mProgram->setUniformValue("uSize", params.width, params.height);
    mProgram->setUniformValue("uOffset", params.offsetX, params.offsetY);
    setColor("uColor0", params.color0);
    setColor("uColor1", params.color1);
    setColor("uLineColor", params.lineColor);

    gl.glDisable(GL_BLEND);
    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    mProgram->release();
}
