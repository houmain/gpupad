Texture2D<float4> SurfacePosition : register(t0);
Texture2D<float4> SurfaceNormal : register(t1);
RWStructuredBuffer<uint> Eligibility : register(u0);

cbuffer Settings : register(b0)
{
    uint fixture;
    uint maskMode;
};

[numthreads(8, 8, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    uint2 size;
    SurfacePosition.GetDimensions(size.x, size.y);
    uint2 pixel = thread.xy;
    if (any(pixel >= size))
        return;

    uint index = pixel.x + size.x * pixel.y;
    bool valid = SurfacePosition.Load(int3(pixel, 0)).w > 0;
    bool selected = SurfaceNormal.Load(int3(pixel, 0)).w >= 0.4;
    if (fixture != 0 && maskMode != 0)
        selected = maskMode == 3 || (maskMode == 2 && index % 3 == 0);
    Eligibility[index] = valid && selected ? 1u : 0u;
}
