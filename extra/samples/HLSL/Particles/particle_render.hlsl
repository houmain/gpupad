
struct VertexOutput {
    float4 position : SV_Position;
};

VertexOutput VS(float2 position : TEXCOORD0) {
    VertexOutput output;
    output.position = float4(position, 0.0, 1.0);
    return output;
}

struct GeometryOutput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

[maxvertexcount(4)]
void GS(point VertexOutput input[1], inout TriangleStream<GeometryOutput> stream) {
    static const float2 offsets[4] = {
        float2(-1.0, -1.0), float2(-1.0, 1.0),
        float2( 1.0, -1.0), float2( 1.0, 1.0)
    };
    static const float2 texCoords[4] = {
        float2(0.0, 1.0), float2(0.0, 0.0),
        float2(1.0, 1.0), float2(1.0, 0.0)
    };

    [unroll]
    for (uint i = 0; i < 4; ++i) {
        GeometryOutput output;
        output.position = input[0].position + float4(offsets[i] / 128.0, 0.0, 0.0);
        output.texCoord = texCoords[i];
        stream.Append(output);
    }
}

float4 PS(GeometryOutput input) : SV_Target0 {
    float alpha = saturate(1.0 - 2.0 * distance(input.texCoord, float2(0.5, 0.5)));
    return float4(0.9, 0.5, 0.2, alpha / 20.0);
}
