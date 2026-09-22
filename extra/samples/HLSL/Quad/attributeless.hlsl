
struct VertexOutput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

VertexOutput main(uint vertexId : SV_VertexID)
{
    static const float2 positions[4] = {
        float2(-1.0, -1.0), float2(1.0, -1.0),
        float2(-1.0,  1.0), float2(1.0,  1.0)
    };

    static const float2 texCoords[4] = {
        float2(0.0, 1.0),
        float2(1.0, 1.0),
        float2(0.0, 0.0),
        float2(1.0, 0.0)
    };

    VertexOutput output;
    output.position = float4(positions[vertexId], 0.0, 1.0);
    output.texCoord = texCoords[vertexId];;
    return output;
}
