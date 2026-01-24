#version 330

uniform int frame;
uniform ivec2 resolution;
out vec4 color;

void main() {
  float interval = 4;
  float frameRate = 60;
  int barWidth = 8;
  int width = resolution.x - barWidth;
  float speed = width / frameRate / interval;
  float pos = abs(int(frame * speed) % (width * 2) - width);
  float onLine =
    step(pos, gl_FragCoord.x) -
    step(pos + barWidth, gl_FragCoord.x);
  color = vec4(0, onLine, 0, 1);
}
