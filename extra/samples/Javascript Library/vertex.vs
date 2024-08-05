#version 330
 
in vec2 aPosition;

void main() {
  gl_Position = vec4(aPosition * 2 - vec2(1), 0.0, 1.0);
}
