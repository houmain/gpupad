
cbuffer Parameters : register(b0) {
    float frame;
};

struct FragmentInput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

void main(FragmentInput input)
{
    float size = 0.03 + sin(frame / 5.0) / 100.0;
    float position = frame / 30.0;
    float radius = frame / 2000.0;
    float2 center = 0.5 + float2(cos(position), sin(position)) * radius;
    clip(size - distance(input.texCoord, center));
}
