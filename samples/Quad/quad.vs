#version 330

in vec2 aPosition;
in vec2 aTexCoords;
out vec2 vTexCoords;

void main() {
	vTexCoords = aTexCoords;
	gl_Position = vec4(aPosition, 0, 1);
}
