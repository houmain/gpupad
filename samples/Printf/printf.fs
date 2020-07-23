#version 330

in vec2 vTexCoords;
uniform ivec2 uMousePosition;
out vec4 oColor;

void main() {
  printfEnabled = (ivec2(gl_FragCoord) == uMousePosition);
  
  vec2 tc = floor(vTexCoords * 10);
  oColor = vec4(sin(tc), 0.33, 1);
    
  printf("The color at %i is %u", uMousePosition, uvec3(oColor.rgb * 255));
}
