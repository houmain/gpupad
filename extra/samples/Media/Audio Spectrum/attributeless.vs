#version 330
 
const vec2 pos[4] = vec2[](
  vec2(-1,-1),
  vec2( 1,-1),
  vec2(-1, 1),
  vec2( 1, 1)
);

const vec2 uv[4] = vec2[](
  vec2(0, 1),
  vec2(1, 1),
  vec2(0, 0),
  vec2(1, 0)
);

out vec2 vTexCoords;

void main() {
  vTexCoords = uv[gl_VertexID];
  gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
}
