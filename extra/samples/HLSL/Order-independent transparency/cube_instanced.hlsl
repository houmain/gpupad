
cbuffer Camera : register(b0) {
    row_major float4x4 viewMatrix;
    row_major float4x4 projectionMatrix;
};

struct VertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float3 worldNormal : NORMAL0;
    float3 worldPosition : POSITION0;
    float3 worldView : TEXCOORD0;
};

VertexOutput main(float3 vertexPosition : TEXCOORD0,
                  float3 vertexNormal : TEXCOORD1,
                  float4 particlePosition : TEXCOORD2,
                  float4 particleColor : TEXCOORD3)
{
    VertexOutput output;
    output.color = particleColor;
    output.worldNormal = -vertexNormal;
    output.worldPosition = particlePosition.xyz + vertexPosition * particlePosition.w;

    float3 worldEyePosition = -mul(viewMatrix[3].xyz, (float3x3)viewMatrix);
    output.worldView = normalize(output.worldPosition - worldEyePosition);
    output.position = mul(float4(output.worldPosition, 1.0), mul(viewMatrix, projectionMatrix));
    return output;
}
