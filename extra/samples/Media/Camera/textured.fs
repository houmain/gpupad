#version 330

uniform sampler2D uTexture;
uniform float uTime;

in vec2 vTexCoords;
out vec4 oColor;

void main() {
  vec2 uv = vTexCoords;
  vec4 color = texture(uTexture, uv);
  float gray = length(color.rgb);
  gray = pow(length(vec2(dFdx(gray), dFdy(gray))), 0.5);
  oColor = vec4(vec3(gray), 1.0);
}
