#version 330

uniform sampler2D uTexture;
uniform float uTime;

in vec2 vTexCoords;
out vec4 oColor;

void main() {
  vec2 uv = vTexCoords;
  uv.y *= 2.1;
  uv.x += mix(0, sin(uv.y * 50 + uTime * 8) * 0.02, clamp(1 - uv.y, 0, 1));
  vec4 color = texture(uTexture, uv);
  color.rgb -= vec3(0.5, 0.2, 0.1) * smoothstep(0, 1, max(1 - uv.y, 0));
  oColor = color;
}
