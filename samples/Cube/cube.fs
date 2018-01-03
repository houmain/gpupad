#version 330

uniform sampler2D uTexture;
in vec2 vTexCoords;
in vec3 vNormal;
out vec4 oColor;

void main() {
  vec3 color = texture(uTexture, vTexCoords).rgb;
  color *= 0.7 + 0.4 * dot(vNormal, vec3(-1, 0, 0));
  oColor = vec4(color, 1);
}
