#version 330

in vec2 aPosition;
in vec2 aInstancePosition;
in vec4 aInstanceColor;

out vec4 vColor;

void main() {
  gl_Position = vec4(aPosition / 3 + aInstancePosition / 2, 0, 1);
  vColor = aInstanceColor;
}
