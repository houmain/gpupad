#version 330

out vec4 oColor;

void main() {
  float a = 1 - 2 * distance(gl_PointCoord, vec2(0.5));
  oColor = vec4(0.9, 0.5, 0.2, a / 20);
}
