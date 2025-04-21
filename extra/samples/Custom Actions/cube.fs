#version 450

uniform vec3 uAmbient;

uniform sampler2D uTexture;

in vec2 vTexCoords;
in vec3 vNormal;
out vec4 oColor;

void main() {
  const vec3 light = normalize(vec3(-2, 3, 1));
  vec3 color = uAmbient + texture(uTexture, vTexCoords).rgb;
  color *= 0.3 + 0.7 * max(dot(vNormal, light), 0.0);
  oColor = vec4(color, 1);
}
