#version 450

layout (location = 0) in PerVertexData
{
  vec4 color;
} fragIn;

layout (location = 0) out vec4 FragColor;

void main()
{
  FragColor = fragIn.color;
}