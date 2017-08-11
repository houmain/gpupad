#version 330

uniform mat4 modelViewProjection;
in vec3 position;

void main() {
  gl_Position = modelViewProjection * vec4(position, 1);
}
