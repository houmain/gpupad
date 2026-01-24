#version 330

in vec2 vTexCoords;
uniform vec2 uMouseFragCoord;
out vec4 oColor;

void main() {
  vec2 tc = floor(vTexCoords * 10);
  oColor = vec4(sin(tc), 0.33, 1);
  
#if defined(GPUPAD)
  if (gl_FragCoord.xy == uMouseFragCoord)
    printf("The color at %i is %u",
      ivec2(uMouseFragCoord), uvec3(oColor.rgb * 255));
#endif  
  
  if (uMouseFragCoord.x == gl_FragCoord.x)
    oColor.g = 1;
  if (uMouseFragCoord.y == gl_FragCoord.y)
    oColor.r = 1;
}
