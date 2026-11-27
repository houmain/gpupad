#include "RayPayload.hlsl"
#include "../shared/Display.hlsl"

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> OutputImage : register(u0);
RWStructuredBuffer<uint4> Probes : register(u1);
RWStructuredBuffer<uint> Capture : register(u2);

cbuffer Settings : register(b0)
{
    uint probeMode;
    uint captureMode;
};

RayPayload trace(float3 origin, float3 direction)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.001;
    ray.TMax = 80.0;
    RayPayload payload = (RayPayload)0;
    // instance-only hit-group indexing (rtgbook, #2, section 15.3.1)
    TraceRay(Scene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray, payload);
    return payload;
}

[shader("raygeneration")]
void RayGeneration()
{
    uint2 pixel = DispatchRaysIndex().xy;
    if (probeMode != 0) {
        float x = 4 * (float(pixel.x) - 1.5) + 0.87;
        float y = pixel.x < 4 ? 0.017 : 3.0;
        RayPayload probe = trace(float3(x, y, -6), float3(0, 0, 1));
        Probes[pixel.x] = uint4(probe.Stamp, probe.Distance > 0 ? 1u : 0u);
        OutputImage[pixel] = float4(probe.Distance > 0 ? float3(0.2, 0.8, 0.4) : 0, 1);
        return;
    }

    uint2 size = DispatchRaysDimensions().xy;
    float2 uv = ((float2(pixel) + 0.5) / float2(size) * 2 - 1) * float2(1, -1);
    uv.x *= float(size.x) / float(size.y);
    float3 origin = float3(0.14, 0.36, -5.0);
    float3 direction = normalize(float3(0.62 * uv + float2(0.07, -0.035), 1));
    RayPayload primary = trace(origin, direction);
    float3 light = 0.38 * primary.Radiance;
    if (primary.Distance > 0) {
        float3 hit = origin + primary.Distance * direction;
        // ideal mirror direction (rtgbook, #2, section 8.1)
        float3 reflectedDirection = reflect(direction, primary.Normal);
        // fixed shading-normal offset (rtgbook, #1, section 6.2.2.3)
        RayPayload reflected = trace(hit + 0.002 * primary.Normal, reflectedDirection);
        // schlick fresnel (rtgbook, #2, section 9.3)
        float grazing = pow(1 - saturate(dot(-direction, primary.Normal)), 5);
        float3 fresnel = lerp(primary.Reflectance, 1.0, grazing);
        light = primary.Radiance + fresnel * reflected.Radiance;
    }

    float3 display = displayColor(light);
    OutputImage[pixel] = float4(display, 1);
    if (captureMode != 0) {
        uint3 rgb = uint3(round(saturate(display) * 255));
        Capture[pixel.x + pixel.y * size.x] = rgb.x | (rgb.y << 8) | (rgb.z << 16) | 0xff000000u;
    }
}
