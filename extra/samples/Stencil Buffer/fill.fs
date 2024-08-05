#version 330

in vec2 vTexCoords;
out vec4 oColor;

uniform float frame;

void main() {
  float time = frame / 60.0;
  float s = sin(time);
  float c = cos(time);
  vec2 pos = abs(vTexCoords.xy * mat2x2(c, s, -s, c) * 2);
  oColor = vec4(pos, 0.3, 1);
}
