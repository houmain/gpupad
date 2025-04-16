#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_ARB_gpu_shader_int64 : enable

const vec2 pos[4] = vec2[](
  vec2(-1,-1),
  vec2( 1,-1),
  vec2(-1, 1),
  vec2( 1, 1)
);

layout(location = 0) out vec3 color;

// The reference to our buffer, aligned to 16 bytes (vec4)
layout(buffer_reference, scalar, buffer_reference_align = 16) buffer VertexColors
{
    vec4 colors[];
};

// Push constant holding the address of our VertexColors buffer
layout(push_constant) uniform PushConstants
{
    uint64_t vertexColors;
}
bufferReferences;

void main()
{
    VertexColors vColors = VertexColors(bufferReferences.vertexColors); // Retrieve Buffer from its address
    color = vColors.colors[gl_VertexIndex].rgb;
    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
}
