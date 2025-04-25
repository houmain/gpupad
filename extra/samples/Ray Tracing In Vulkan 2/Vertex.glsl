
struct Vertex
{
  vec3 Position;
  vec3 Normal;
  vec2 TexCoord;
};

Vertex UnpackVertex(uint index)
{
	const uint vertexSize = 8;
	const uint offset = index * vertexSize;
	
	Vertex v;
	
	v.Position = vec3(Vertices[offset + 0], Vertices[offset + 1], Vertices[offset + 2]);
	v.Normal = vec3(Vertices[offset + 3], Vertices[offset + 4], Vertices[offset + 5]);
	v.TexCoord = vec2(Vertices[offset + 6], Vertices[offset + 7]);

	return v;
}
