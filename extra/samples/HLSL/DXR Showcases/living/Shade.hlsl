#include "Scene.hlsl"
#include "../shared/Display.hlsl"

RaytracingAccelerationStructure Scene : register(t0);
Texture2D<float4> HitPosition : register(t1);
Texture2D<float4> HitNormal : register(t2);
Texture2D<float4> ReflectedLight : register(t3);
RWTexture2D<float4> OutputImage : register(u0);
RWStructuredBuffer<uint> ShadowProbes : register(u1);

cbuffer Controls : register(b0)
{
    uint captureProbes;
};

bool isOccluded(float3 position, float3 normal)
{
    RayDesc ray;
    ray.Origin = position + rayBias * normal;
    float3 toLight = lightPosition - ray.Origin;
    float lengthToLight = length(toLight);
    if (lengthToLight <= 2 * rayBias)
        return false;

    ray.Direction = toLight / lengthToLight;
    ray.TMin = rayBias;
    ray.TMax = lengthToLight - rayBias;
    // first opaque triangle blocker in the light segment (dxr 1.1 rayquery)
    RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
    query.TraceRayInline(Scene, RAY_FLAG_NONE, 0xff, ray);
    while (query.Proceed()) {
    }
    return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

[numthreads(8, 8, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    uint2 size;
    OutputImage.GetDimensions(size.x, size.y);
    uint2 pixel = thread.xy;
    if (any(pixel >= size))
        return;

    if (captureProbes != 0 && pixel.y == 0 && pixel.x < 3) {
        const float2 targets[3] = { float2(-0.48, 2.90), float2(-0.66, 2.18),
            float2(-0.48, 1.64) };
        float2 floorXZ = targets[pixel.x];
        ShadowProbes[pixel.x] =
            isOccluded(float3(floorXZ.x, 0, floorXZ.y), float3(0, 1, 0)) ? 1u : 0u;
    }

    float4 surface = HitPosition.Load(int3(pixel, 0));
    float3 light;
    if (surface.w > 0) {
        float4 data = HitNormal.Load(int3(pixel, 0));
        float3 normal = normalize(data.xyz);
        uint material = uint(data.w);
        float3 albedo = baseColor(material, surface.xyz);
        float3 key = keyLighting(albedo, surface.xyz, normal);
        if (any(key > 0) && isOccluded(surface.xyz, normal))
            key = 0;
        light = fillLighting(albedo, normal) + key + ReflectedLight.Load(int3(pixel, 0)).rgb;
        if (material == floorMaterial) {
            float3 background = 0.45 * studio(normalize(surface.xyz - cameraOrigin));
            light = lerp(light, background, smoothstep(26, sceneRange, surface.w));
        }
    } else {
        light = 0.45 * studio(cameraDirection(pixel, size));
    }
    OutputImage[pixel] = float4(displayColor(light), 1);
}
