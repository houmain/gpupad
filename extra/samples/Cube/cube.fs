#version 450

#if defined(GPUPAD_VULKAN)

layout(push_constant, std430) uniform pc {
  layout(offset=192) vec3 uAmbient;
};

#else

uniform vec3 uAmbient;

#endif

uniform sampler2D uTexture;

in vec2 vTexCoords;
in vec3 vNormal;
out vec4 oColor;

void main() {
  const vec3 light = vec3(-0.71, 0.71, 0);
  vec3 color = uAmbient + texture(uTexture, vTexCoords).rgb;
  color *= 0.1 + 0.9 * max(dot(vNormal, light), 0.0);
  oColor = vec4(color, 1);
}
