#version 330

in vec2 vTexCoords;
uniform vec2 uMouseFragCoord;
out vec4 oColor;

void main() {
  printfEnabled = (gl_FragCoord.xy == uMouseFragCoord);
  
  vec2 tc = floor(vTexCoords * 10);
  oColor = vec4(sin(tc), 0.33, 1);
    
  printf("The color at %i is %u", ivec2(uMouseFragCoord), uvec3(oColor.rgb * 255));
}
