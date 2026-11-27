#include "Scene.hlsl"
#include "../shared/StudioTrace.hlsl"

RWTexture2D<float4> HitPosition : register(u0);
RWTexture2D<float4> HitNormal : register(u1);
RWTexture2D<float4> ReflectedLight : register(u2);
RWStructuredBuffer<uint2> GeometryProbes : register(u3);

cbuffer Controls : register(b0)
{
    uint captureProbes;
};

[shader("raygeneration")]
void RayGeneration()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 size = DispatchRaysDimensions().xy;
    if (captureProbes != 0 && pixel.y == 0 && pixel.x < 4) {
        const float2 targets[4] = { float2(-1.82, 1.68), float2(0.13, 1.84),
            float2(2.48, 0.80), float2(0, 3.5) };
        RayPayload probe = traceTriangles(
            float3(targets[pixel.x], -6), float3(0, 0, 1), sceneRange);
        GeometryProbes[pixel.x] = uint2(probe.Instance, asuint(probe.Distance));
    }

    float3 direction = cameraDirection(pixel, size);
    RayPayload surface = traceSurface(cameraOrigin, direction);
    if (surface.Distance == 0) {
        HitPosition[pixel] = 0;
        HitNormal[pixel] = 0;
        ReflectedLight[pixel] = 0;
        return;
    }

    float3 position = cameraOrigin + surface.Distance * direction;
    // ideal mirror direction (rtgbook, #2, section 8.1)
    float3 reflectedDirection = reflect(direction, surface.Normal);
    float3 reflectedOrigin = position + rayBias * surface.Normal;
    RayPayload secondary = traceSurface(reflectedOrigin, reflectedDirection);
    float3 incoming = studio(reflectedDirection);
    if (secondary.Distance > 0) {
        float3 secondaryPosition = reflectedOrigin + secondary.Distance * reflectedDirection;
        float3 color = baseColor(secondary.Instance, secondaryPosition);
        incoming = fillLighting(color, secondary.Normal)
            + keyLighting(color, secondaryPosition, secondary.Normal);
    }

    float3 f0 = surface.Instance == floorMaterial ? 0.09
        : 0.67 * baseColor(surface.Instance, position);
    // schlick fresnel (rtgbook, #2, section 9.3)
    float grazing = pow(1 - saturate(dot(-direction, surface.Normal)), 5);
    HitPosition[pixel] = float4(position, surface.Distance);
    HitNormal[pixel] = float4(surface.Normal, float(surface.Instance));
    ReflectedLight[pixel] = float4(lerp(f0, 1.0, grazing) * incoming, 1);
}
