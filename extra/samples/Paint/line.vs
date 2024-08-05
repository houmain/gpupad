#version 330

uniform vec2 uMouseCoord;
uniform vec2 uPrevMouseCoord;
uniform float uSize;
out vec2 vTexCoords;

void main() {
  vec2 coord[] = vec2[](uPrevMouseCoord, uMouseCoord);  
  float size = uSize / (2 * length(coord[0] - coord[1]));    
  vec2 dir = (coord[0] - coord[1]) * size;
  coord[0] += dir;
  coord[1] -= dir;
  vec2 perp[] = vec2[](vec2(-dir.y, dir.x), vec2(dir.y, -dir.x));
  vec2 pos = coord[gl_VertexID % 2] + perp[gl_VertexID / 2];
  gl_Position = vec4(pos, 0.0, 1.0);
  
  const vec2 uv[] = vec2[](
    vec2(0, 1),
    vec2(0, 0),
    vec2(1, 1),
    vec2(1, 0));  
  vTexCoords = uv[gl_VertexID];
}
