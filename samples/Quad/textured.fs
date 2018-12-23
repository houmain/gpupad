#version 330

uniform sampler2D uTexture;
in vec2 vTexCoords;
out vec4 oColor;

void main() {
	vec4 color = texture(uTexture, vTexCoords);
	color *= vec4(cos(vTexCoords), 1.0, 0.75);
	oColor = color;
}
