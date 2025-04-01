#version 450
#extension GL_ARB_bindless_texture : require

layout(std430) readonly buffer bTextures {
  sampler2D textures[];
};

in vec2 vTexCoords;
out vec4 oColor;

void main() {
  uint index = uint(vTexCoords.x * 10) % textures.length();
  oColor = texture(textures[index], vTexCoords);
}
