#include "RayPayload.hlsl"

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> OutputImage : register(u0);

[shader("raygeneration")]
void RayGeneration()
{
    uint2 index = DispatchRaysIndex().xy;
    uint2 dimensions = DispatchRaysDimensions().xy;
    float2 uv = (float2(index) + 0.5) / float2(dimensions) * 2.0 - 1.0;
    uv.x *= float(dimensions.x) / float(dimensions.y);

    RayDesc ray;
    ray.Origin = float3(uv.x, -uv.y, -2.0);
    ray.TMin = 0.001;
    ray.Direction = float3(0.0, 0.0, 1.0);
    ray.TMax = 100.0;

    RayPayload payload;
    payload.Color = 0.0;
    TraceRay(Scene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray, payload);
    OutputImage[index] = float4(payload.Color, 1.0);
}
