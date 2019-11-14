#version 330

in vec4 aPosition;

void main() {
  gl_PointSize = max(aPosition.z * 32, 4);
  gl_Position = aPosition;
}
