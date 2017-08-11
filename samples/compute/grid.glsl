#version 430

layout (rgba8) uniform image2D image;
layout (local_size_x = 16, local_size_y = 16) in;

void main() {
  ivec2 size = imageSize(image);
  ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
  if (length(pos - size / 2) > size.x * 3 / 7) {
    vec4 color = imageLoad(image, pos);
    imageStore(image, pos, 
      vec4(1 - color.rgb, color.a));
 }
}
