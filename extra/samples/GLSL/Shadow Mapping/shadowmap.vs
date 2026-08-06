#version 330 core

in vec3 aPosition;

uniform mat4 uLightProjection;
uniform mat4 uLightView;
uniform mat4 uModel;

void main() {
  gl_Position = uLightProjection * uLightView * uModel * vec4(aPosition, 1);
}
