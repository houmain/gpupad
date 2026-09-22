
struct VertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
};

struct PrimitiveOutput {
    uint primitiveId : SV_PrimitiveID;
};

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main(
    out vertices VertexOutput vertices[3],
    out indices uint3 triangles[1],
    out primitives PrimitiveOutput primitives[1])
{
    SetMeshOutputCounts(3, 1);

    vertices[0].position = float4(-0.95, -0.95, 0.5, 1.0);
    vertices[1].position = float4( 0.00,  0.95, 0.5, 1.0);
    vertices[2].position = float4( 0.95, -0.95, 0.5, 1.0);
    vertices[0].color = float4(1.0, 0.0, 0.0, 1.0);
    vertices[1].color = float4(0.0, 1.0, 0.0, 1.0);
    vertices[2].color = float4(0.0, 0.0, 1.0, 1.0);
    triangles[0] = uint3(0, 1, 2);
    primitives[0].primitiveId = 0;
}
