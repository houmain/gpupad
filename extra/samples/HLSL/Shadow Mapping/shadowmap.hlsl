
cbuffer Transforms : register(b0) {
    row_major float4x4 uModel;
    row_major float4x4 uView;
    row_major float4x4 uProjection;
    row_major float4x4 uLightView;
    row_major float4x4 uLightProjection;
};

float4 main(float3 position : TEXCOORD0) : SV_Position
{
    return mul(float4(position, 1.0), mul(uModel, mul(uLightView, uLightProjection)));
}
