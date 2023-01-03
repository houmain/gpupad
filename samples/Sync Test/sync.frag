#version 330

uniform int frame;
uniform sampler2D target;
out vec4 color;

void main() {
  float interval = 4;
  float frameRate = 60;
  int barWidth = 8;
  int width = textureSize(target, 0).x - barWidth;
  float speed = width / frameRate / interval;
  float pos = abs(int(frame * speed) % (width * 2) - width);
  float onLine =
    step(pos, gl_FragCoord.x) -
    step(pos + barWidth, gl_FragCoord.x);
  color = vec4(0, onLine, 0, 1);
}
