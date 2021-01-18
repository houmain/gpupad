#version 330

in vec2 vTexCoords;

uniform float frame;

void main() {
  float size = 0.03 + sin(frame / 5) / 100.0;
  float pos = frame / 30;
  float radius = frame / 2000;
  vec2 center = vec2(0.5) + vec2(cos(pos), sin(pos)) * radius;
  if (distance(vTexCoords, center) > size)
    discard;
}
