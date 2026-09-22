
Texture2D<float4> uTexture : register(t0);
SamplerState uSampler : register(s0);

cbuffer Color : register(b0) {
    float4 uColor;
};

struct FragmentInput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

float4 main(FragmentInput input) : SV_Target0
{
    return uTexture.Sample(uSampler, input.texCoord) * uColor;
}
