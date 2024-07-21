#include "TextureBackground.h"
#include "GLWidget.h"
#include <QOpenGLShaderProgram>

namespace {
    const auto vertexShaderSource = R"(
#version 330

const vec2 data[4]= vec2[] (
  vec2(-1.0,  1.0),
  vec2(-1.0, -1.0),
  vec2( 1.0,  1.0),
  vec2( 1.0, -1.0)
);

void main() {
  vec2 pos = data[gl_VertexID];
  gl_Position = vec4(pos, 0.0, 1.0);
}
)";

    const auto fragmentShaderSource = R"(
#version 330

uniform vec4 uColor0;
uniform vec4 uColor1;
uniform vec4 uLineColor;
uniform vec2 uOffset;
uniform vec2 uSize;

out vec4 oColor;

void main() {
  vec2 s = (gl_FragCoord.xy - uOffset) / 16;
  float m = max(sign(mod(floor(s.x) + floor(s.y), 2.0)), 0.0);
  vec4 color = mix(uColor0, uColor1, m);
  
  vec2 d0 = (gl_FragCoord.xy - uOffset);
  vec2 d1 = (uOffset + uSize - gl_FragCoord.xy);
  float outside =
    step(-1, d0.x) * step(-1, d1.x) *
    step(-1, d0.y) * step(-1, d1.y);
  float inside =
    step(0, d0.x) * step(0, d1.x) *
    step(0, d0.y) * step(0, d1.y);
    
  oColor = mix(color, uLineColor, outside - inside);
}
)";

    bool buildProgram(QOpenGLShaderProgram &program)
    {
        program.create();
        auto vertexShader = new QOpenGLShader(QOpenGLShader::Vertex, &program);
        auto fragmentShader =
            new QOpenGLShader(QOpenGLShader::Fragment, &program);
        vertexShader->compileSourceCode(vertexShaderSource);
        fragmentShader->compileSourceCode(fragmentShaderSource);
        program.addShader(vertexShader);
        program.addShader(fragmentShader);
        return program.link();
    }
} // namespace

TextureBackground::TextureBackground(GLWidget *parent) : QObject(parent) { }

TextureBackground::~TextureBackground() = default;

GLWidget &TextureBackground::widget()
{
    return *qobject_cast<GLWidget *>(parent());
}

void TextureBackground::releaseGL()
{
    mProgram.reset();
}

void TextureBackground::paintGL(const QSizeF &size, const QPointF &offset)
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);
    auto &gl = widget().gl();

    if (!mProgram) {
        mProgram.reset(new QOpenGLShaderProgram());
        buildProgram(*mProgram);
    }

    const auto color = QPalette().window().color();
    const auto color0 = color.darker(103);
    const auto color1 = color.lighter(103);

    mProgram->bind();
    mProgram->setUniformValue("uSize", size);
    mProgram->setUniformValue("uOffset", offset);
    mProgram->setUniformValue("uColor0", color0);
    mProgram->setUniformValue("uColor1", color1);
    mProgram->setUniformValue("uLineColor", QColor(Qt::gray));

    gl.glDisable(GL_BLEND);
    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    mProgram->release();
}
