#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform sampler uSampler;
layout(binding = 1) uniform texture2D uTexture[];

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 oColor;

void main() {
  const int index = int(vTexCoords.x * 2);
  oColor = texture(
    nonuniformEXT(sampler2D(uTexture[index], uSampler)),
    vTexCoords);
}
