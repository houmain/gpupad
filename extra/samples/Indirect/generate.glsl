#version 430

layout (local_size_x = 16, local_size_y = 16) in;

layout(std430, binding=0) buffer uVertexBuffer {
  vec2 positions[];
};

uniform int uCountX;
uniform int uCountY;

void main() {
  uint x = gl_GlobalInvocationID.x;
  uint y = gl_GlobalInvocationID.y;
  if (x >= uCountX || y >= uCountY)
    return;

  uint offset = (y * uCountX + x) * 6;
  
  float sx = 2.0f / uCountX;
  float sy = 2.0f / uCountY; 
  vec2 v0 = vec2((x + 0) * sx, (y + 0) * sy) - vec2(1);
  vec2 v1 = vec2((x + 1) * sx, (y + 0) * sy) - vec2(1);
  vec2 v2 = vec2((x + 0) * sx, (y + 1) * sy) - vec2(1);
  vec2 v3 = vec2((x + 1) * sx, (y + 1) * sy) - vec2(1);
   
  float indent = 0.3;
   
  vec2 mid = (v0 + v1 + v2) / 3;
  positions[offset + 0] = mix(v0, mid, indent);
  positions[offset + 1] = mix(v1, mid, indent);
  positions[offset + 2] = mix(v2, mid, indent);
  
  mid = (v1 + v2 + v3) / 3;
  positions[offset + 4] = mix(v1, mid, indent);
  positions[offset + 3] = mix(v3, mid, indent);
  positions[offset + 5] = mix(v2, mid, indent);
}
