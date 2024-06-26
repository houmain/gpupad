#version 330

in vec2 vTexCoords;
uniform vec2 uMouseFragCoord;
out vec4 oColor;

void main() {
  
#if defined(GPUPAD)
  printfEnabled = (gl_FragCoord.xy == vec2(0.5, 0.5));
  printf("Hello World");

  printfEnabled = (gl_FragCoord.xy == uMouseFragCoord);
  printf("The color at %i is %u", ivec2(uMouseFragCoord), uvec3(oColor.rgb * 255));
#endif
  
  vec2 tc = floor(vTexCoords * 10);
  oColor = vec4(sin(tc), 0.33, 1);
  
  if (uMouseFragCoord.x == gl_FragCoord.x)
    oColor.g = 1;
  if (uMouseFragCoord.y == gl_FragCoord.y)
    oColor.r = 1;
}
