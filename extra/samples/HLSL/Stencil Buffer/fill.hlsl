
cbuffer Parameters : register(b0) {
    float frame;
};

struct FragmentInput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

float4 main(FragmentInput input) : SV_Target0
{
    float time = frame / 60.0;
    float sine = sin(time);
    float cosine = cos(time);
    float2x2 rotation = float2x2(cosine, -sine, sine, cosine);
    float2 position = abs(mul(input.texCoord, rotation) * 2.0);
    return float4(position, 0.3, 1.0);
}
