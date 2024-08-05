#version 330

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

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
