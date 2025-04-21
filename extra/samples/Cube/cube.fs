#version 450

uniform vec3 uAmbient;

uniform sampler2D uTexture;

in vec2 vTexCoords;
in vec3 vNormal;
out vec4 oColor;

vec3 toLinear(vec3 color) {
  return color * color;
}

void main() {
  const vec3 light = normalize(vec3(-1, 3, 2));
  vec3 diffuse = toLinear(texture(uTexture, vTexCoords).rgb)
    * max(dot(vNormal, light), 0.0);
  vec3 ambient = toLinear(uAmbient);
  vec3 color = ambient + diffuse;
  oColor = vec4(color, 1);
}
