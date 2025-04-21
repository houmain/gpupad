#version 450

uniform mat4 uView;
uniform mat4 uProjection;

in vec3 aPosition;
in vec3 aNormal;
in vec2 aTexCoords;

out vec2 vTexCoords;
out vec3 vNormal;

void main() {
  vTexCoords = aTexCoords;
  vNormal = aNormal;
  gl_Position = uProjection * uView * vec4(aPosition, 1);
}
