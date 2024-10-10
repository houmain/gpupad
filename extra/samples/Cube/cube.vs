#version 450

#if defined(GPUPAD_VULKAN)

layout(push_constant, std430) uniform pc {
  mat4 uModel;
  mat4 uView;
  mat4 uProjection;
};

#else

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

#endif

in vec3 aPosition;
in vec3 aNormal;
in vec2 aTexCoords;

out vec2 vTexCoords;
out vec3 vNormal;

void main() {
  vTexCoords = aTexCoords;
  vNormal = (uModel * vec4(aNormal, 0)).xyz;
  gl_Position = uProjection * uView * uModel * vec4(aPosition, 1);
}
