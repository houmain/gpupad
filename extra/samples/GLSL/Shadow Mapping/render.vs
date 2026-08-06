#version 330

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightView;
uniform mat4 uLightProjection;

in vec3 aPosition;
in vec2 aTexCoords;
in vec3 aNormal;

out vec2 vTexCoords;
out vec3 vNormal;
out vec3 vWorldPosition;
out vec4 vShadowCoord;

void main() {
  vTexCoords = aTexCoords;
  vNormal = (uModel * vec4(aNormal, 0)).xyz;
  vWorldPosition = (uModel * vec4(aPosition, 1)).xyz;
  
  const mat4 bias = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 0.5, 0.0,
    0.5, 0.5, 0.5, 1.0
  );
  
  vShadowCoord = bias * uLightProjection * uLightView * vec4(vWorldPosition, 1);
  gl_Position = uProjection * uView * vec4(vWorldPosition, 1);
}
