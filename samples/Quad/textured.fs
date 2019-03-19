#version 330

uniform sampler2D uTexture;
in vec2 vTexCoords;
out vec4 oColor;

void main() {
  vec2 tc = vTexCoords;
  vec4 color = vec4(sin(tc), 0.33, tc * 5);
  oColor = color * texture(uTexture, vTexCoords);
}
