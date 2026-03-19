#version 330

uniform float time;
uniform ivec2 resolution;
out vec4 color;

void main() {
  float interval = 2;
  int barWidth = 8;
  int width = resolution.x - barWidth;
  float speed = width / interval;
  float pos = abs(int(time * speed) % (width * 2) - width);
  float onLine =
    step(pos, gl_FragCoord.x) -
    step(pos + barWidth, gl_FragCoord.x);
  color = vec4(0, onLine, 0, 1);
}
