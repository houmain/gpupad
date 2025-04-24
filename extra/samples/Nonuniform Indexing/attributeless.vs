#version 450
 
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

layout(location = 0) out vec2 vTexCoords;

void main() {
  vTexCoords = uv[gl_VertexIndex];
  gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
}
