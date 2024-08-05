#version 330

uniform sampler2D uTexture;
uniform vec4 uColor;

in vec2 vTexCoords;
out vec4 oColor;

void main() {
  oColor = texture(uTexture, vTexCoords) * uColor;
}
