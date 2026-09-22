
RWTexture3D<unorm float> uImage : register(u0);

cbuffer Animation : register(b0) {
    float frame;
};

float sdTorus(float3 p, float2 t)
{
    float2 q = float2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

float3x3 rotateX(float angle)
{
    float s;
    float c;
    sincos(angle, s, c);
    return float3x3(1.0, 0.0, 0.0,
                    0.0, c,   -s,
                    0.0, s,    c);
}

[numthreads(16, 16, 1)]
void main(uint3 coordinate : SV_DispatchThreadID)
{
    float3 position = float3(coordinate) / 128.0 - float3(0.5, 0.5, 0.0);
    position = mul(position, rotateX(frame / 60.0));

    float outer = sdTorus(position, float2(0.3, 0.2));
    float inner = sdTorus(position, float2(0.3, 0.15));
    float distanceValue = max(-inner, outer);
    float antialiasWidth = 1.0 / 128.0;
    uImage[coordinate] = smoothstep(antialiasWidth, -antialiasWidth, distanceValue);
}
