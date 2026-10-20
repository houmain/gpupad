
float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    static const float3 positions[6] = {
        float3(-1.0, -1.0, 0.5), float3(-1.0, 1.0, 0.5),
        float3( 1.0,  1.0, 0.5), float3( 1.0, 1.0, 0.5),
        float3( 1.0, -1.0, 0.5), float3(-1.0,-1.0, 0.5)
    };
    return float4(positions[vertexId], 1.0);
}
