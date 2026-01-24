#version 330

uniform vec4 uColor;
uniform float uMousePressed;

in vec2 vTexCoords;
out vec4 oColor;

void main() {
  oColor = uColor;  
  vec2 uv = vTexCoords;
  float vignette = uv.x * uv.y * (1 - uv.x) * (1 - uv.y); 
  oColor.a = uMousePressed * smoothstep(0, 0.05, vignette);
}
