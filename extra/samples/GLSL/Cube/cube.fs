#version 450

uniform vec3 uAmbient;

uniform sampler2D uTexture;

in vec2 vTexCoords;
in vec3 vNormal;
out vec4 oColor;

void main() {
  const vec3 light = normalize(vec3(-1, 3, 2));
  const vec3 normal = normalize(vNormal);
  vec3 albedo = texture(uTexture, vTexCoords).rgb;
  vec3 diffuse = vec3(1) * max(dot(normal, light), 0.0);
  vec3 color = albedo * (uAmbient + diffuse);
  oColor = vec4(color, 1);
}
