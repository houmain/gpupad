#version 330

uniform int frame;
uniform sampler2D target;
out vec4 color;

void main()
{
  float frameRate = 60;
  int barWidth = 8;
  int width = textureSize(target, 0).x - barWidth;
  float speed = width / frameRate;
  int pos = abs(int(frame * speed) % (width * 2) - width);
  float onLine =
    step(pos, gl_FragCoord.x) -
    step(pos + barWidth, gl_FragCoord.x);
  color = mix(
    vec4(vec3(frame % 256 / 255.0), 0.85),
    vec4(0, 1, 0, 1),
    onLine);
}
