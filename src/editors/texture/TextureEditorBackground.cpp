#include "TextureEditorBackground.h"
#include <QPalette>

QString TextureEditorBackground::vertexShaderSource = R"(
#version 330

const vec2 data[4] = vec2[] (
  vec2(-1.0,  1.0),
  vec2(-1.0, -1.0),
  vec2( 1.0,  1.0),
  vec2( 1.0, -1.0)
);

void main() {
#if defined(VULKAN)
  vec2 pos = data[gl_VertexIndex];
#else
  vec2 pos = data[gl_VertexID];
#endif
  gl_Position = vec4(pos, 0.0, 1.0);
}
)";

QString TextureEditorBackground::fragmentShaderSource = R"(
#version 330
#if defined(VULKAN)
layout(push_constant) uniform Params {
  vec4 color0;
  vec4 color1;
  vec4 lineColor;
  vec2 offset;
  vec2 size;
} pc;
layout(location = 0) out vec4 oColor;
#else // !defined(VULKAN)

uniform vec4 uColor0;
uniform vec4 uColor1;
uniform vec4 uLineColor;
uniform vec2 uOffset;
uniform vec2 uSize;
struct Params {
  vec4 color0;
  vec4 color1;
  vec4 lineColor;
  vec2 offset;
  vec2 size;
};
Params pc = Params(uColor0, uColor1, uLineColor, uOffset, uSize);
out vec4 oColor;
#endif // !defined(VULKAN)

void main() {
  vec2 s = (gl_FragCoord.xy - pc.offset) / 16;
  float m = max(sign(mod(floor(s.x) + floor(s.y), 2.0)), 0.0);
  vec4 color = mix(pc.color0, pc.color1, m);

  vec2 d0 = (gl_FragCoord.xy - pc.offset);
  vec2 d1 = (pc.offset +  pc.size - gl_FragCoord.xy);
  float outside =
    step(-1, d0.x) * step(-1, d1.x) *
    step(-1, d0.y) * step(-1, d1.y);
  float inside =
    step(0, d0.x) * step(0, d1.x) *
    step(0, d0.y) * step(0, d1.y);

  oColor = mix(color, pc.lineColor, outside - inside);
}
)";

TextureEditorBackground::TextureEditorBackground(QObject *parent)
    : QObject(parent)
{
}

auto TextureEditorBackground::getParams(const QSizeF &size,
    const QPointF &offset) -> Params
{
    const auto toColor = [](const QColor &color) {
        return Color{ color.redF(), color.greenF(), color.blueF(),
            color.alphaF() };
    };

    const auto color = QPalette().window().color();
    return {
        .color0 = toColor(color.darker(103)),
        .color1 = toColor(color.lighter(103)),
        .lineColor = toColor(QColor(Qt::gray)),
        .offsetX = static_cast<float>(offset.x()),
        .offsetY = static_cast<float>(offset.y()),
        .width = static_cast<float>(size.width()),
        .height = static_cast<float>(size.height()),
    };
}
