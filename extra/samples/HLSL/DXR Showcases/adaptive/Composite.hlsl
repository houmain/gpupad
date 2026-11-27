#include "Scene.hlsl"
#include "../shared/Display.hlsl"

Texture2D<float4> SurfacePosition : register(t0);
Texture2D<float4> SurfaceNormal : register(t1);
StructuredBuffer<uint> Eligibility : register(t2);
StructuredBuffer<float4> Reflections : register(t3);
RWTexture2D<float4> OutputImage : register(u0);

cbuffer Settings : register(b0)
{
    uint fixture;
    uint maskView;
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
    float4 position = SurfacePosition.Load(int3(pixel, 0));
    float3 origin, direction;
    cameraRay(pixel, size, fixture != 0, origin, direction);
    float3 light = 0.45 * studio(direction);
    if (position.w > 0) {
        uint material = uint(position.w) - 1;
        float4 data = SurfaceNormal.Load(int3(pixel, 0));
        float3 direct = surfaceLighting(material, data.w, position.xyz, normalize(data.xyz));
        float3 reflected = Reflections[index].rgb;
        float fade = material == floorMaterial
            ? smoothstep(26, sceneRange, length(position.xyz - origin))
            : 0;
        light = lerp(direct + reflected, light, fade);
    }

    float3 display = displayColor(light);
    if (maskView != 0) {
        float intensity = dot(display, float3(0.21, 0.72, 0.07));
        display = Eligibility[index] != 0 ? float3(0.10, 0.64, 0.61) + 0.27 * intensity
            : 0.36 * intensity;
    }
    OutputImage[pixel] = float4(display, 1);
}
