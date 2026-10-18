#version 450

uniform sampler2D uMipTexture;

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 oColor;

void main()
{
    // Perspective derivatives select progressively smaller mip levels toward
    // the horizon. Nearest mip filtering keeps each level as a distinct band.
    oColor = texture(uMipTexture, vTexCoord);
}
