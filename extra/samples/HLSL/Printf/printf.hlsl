
cbuffer Parameters : register(b0) {
    float2 uMouseFragCoord;
};

float4 main(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    float2 cell = floor(texCoord * 10.0);
    float4 color = float4(sin(cell), 0.33, 1.0);

    if (all(uMouseFragCoord == position.xy))
        printf("The color at %i is %u", int2(uMouseFragCoord), uint3(color.rgb * 255.0));

    if (uMouseFragCoord.x == position.x)
        color.g = 1.0;
    if (uMouseFragCoord.y == position.y)
        color.r = 1.0;
    return color;
}
