
cbuffer Transforms : register(b0) {
    row_major float4x4 uModel;
    row_major float4x4 uView;
    row_major float4x4 uProjection;
    row_major float4x4 uLightView;
    row_major float4x4 uLightProjection;
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    float4 shadowCoord : TEXCOORD1;
};

VertexOutput main(float3 position : TEXCOORD0,
                  float2 texCoord : TEXCOORD1,
                  float3 normal : TEXCOORD2)
{
    static const row_major float4x4 bias = {
        0.5, 0.0, 0.0, 0.0,
        0.0, -0.5, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.5, 0.5, 0.0, 1.0
    };

    VertexOutput output;
    output.texCoord = texCoord;
    output.normal = mul(float4(normal, 0.0), uModel).xyz;
    output.worldPosition = mul(float4(position, 1.0), uModel).xyz;
    output.shadowCoord = mul(float4(output.worldPosition, 1.0),
                             mul(uLightView, mul(uLightProjection, bias)));
    output.position = mul(float4(output.worldPosition, 1.0), mul(uView, uProjection));
    return output;
}
