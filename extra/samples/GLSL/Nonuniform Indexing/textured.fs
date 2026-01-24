#version 450
#extension GL_EXT_nonuniform_qualifier : require

// https://raw.githubusercontent.com/KhronosGroup/GLSL/refs/heads/main/extensions/ext/GL_EXT_nonuniform_qualifier.txt

// https://github.com/KhronosGroup/Vulkan-Samples/blob/main/shaders/descriptor_indexing/glsl/nonuniform-quads.frag

layout(binding = 0) uniform sampler2D uTexture[];

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 oColor;

void main() {
  const int index = int(vTexCoords.x * 2);
  oColor = texture(uTexture[nonuniformEXT(index)], vTexCoords);
}
