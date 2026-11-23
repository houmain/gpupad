#include "Scene.hlsl"
#include "../shared/StudioTrace.hlsl"

Texture2D<float4> SurfacePosition : register(t3);
Texture2D<float4> SurfaceNormal : register(t4);
StructuredBuffer<uint> Eligibility : register(t5);
StructuredBuffer<uint> PixelQueue : register(t6);
RWStructuredBuffer<float4> Reflections : register(u0);

cbuffer Settings : register(b0)
{
    uint fixture;
    uint useQueue;
};

float4 traceReflection(uint2 pixel, uint2 size)
{
    float4 position = SurfacePosition.Load(int3(pixel, 0));
    float4 data = SurfaceNormal.Load(int3(pixel, 0));
    uint material = uint(position.w) - 1;
    float3 normal = normalize(data.xyz);
    float3 camera, incident;
    cameraRay(pixel, size, fixture != 0, camera, incident);
    // ideal mirror direction (rtgbook, #2, section 8.1)
    float3 direction = reflect(incident, normal);
    float3 origin = position.xyz + rayBias * normal;

    RayPayload hit = traceSurface(origin, direction);
    float3 incoming = studio(direction);
    if (hit.Distance > 0) {
        float3 secondary = origin + hit.Distance * direction;
        incoming = surfaceLighting(hit.Instance,
            polish(hit.Instance, secondary, fixture != 0), secondary, hit.Normal);
    }

    float3 color = material == floorMaterial ? 1.0 : materialColor(material, data.w);
    // schlick fresnel (rtgbook, #2, section 9.3)
    float grazing = pow(1 - saturate(dot(-incident, normal)), 5);
    float3 fresnel = lerp(data.w * color, 1.0, grazing);
    float tag = hit.Distance > 0 ? float(2 + hit.Instance) : 1.0;
    return float4(fresnel * incoming, tag);
}

[shader("raygeneration")]
void RayGeneration()
{
    uint2 size;
    SurfacePosition.GetDimensions(size.x, size.y);
    uint2 launch = DispatchRaysIndex().xy;
    uint index;
    if (useQueue != 0) {
        index = PixelQueue[launch.x];
    } else {
        index = launch.x + size.x * launch.y;
        if (Eligibility[index] == 0)
            return;
    }

    uint2 pixel = uint2(index % size.x, index / size.x);
    Reflections[index] = traceReflection(pixel, size);
}
