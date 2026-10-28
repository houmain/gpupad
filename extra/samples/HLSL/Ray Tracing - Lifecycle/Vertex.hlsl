
struct Vertex {
    float3 Position;
    float3 Normal;
    float2 TexCoord;
};

Vertex UnpackVertex(uint index)
{
    static const uint vertexSize = 8 * 4;
    uint offset = index * vertexSize;

    Vertex vertex;
    vertex.Position = asfloat(Vertices.Load3(offset));
    vertex.Normal = asfloat(Vertices.Load3(offset + 3 * 4));
    vertex.TexCoord = asfloat(Vertices.Load2(offset + 6 * 4));
    return vertex;
}
