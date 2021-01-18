#version 330

in vec2 vTexCoords;
out vec4 oColor;

uniform float frame;

void main() {
  vec3 color = mix(vec3(1, 1, 0), vec3(0, 1, 1), vec3(abs(sin(frame / 30.0))));
  oColor = vec4(color, 1);
}
