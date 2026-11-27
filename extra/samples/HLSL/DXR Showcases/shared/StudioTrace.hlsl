#include "StudioPayload.hlsl"

RaytracingAccelerationStructure Scene : register(t0);

RayPayload traceTriangles(float3 origin, float3 direction, float limit)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = rayBias;
    ray.TMax = limit;
    RayPayload payload = (RayPayload)0;
    TraceRay(Scene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray, payload);
    return payload;
}

RayPayload traceSurface(float3 origin, float3 direction)
{
    float floorDistance = direction.y < -1e-4 ? -origin.y / direction.y : 0;
    bool hasFloor = floorDistance > rayBias && floorDistance < sceneRange;
    RayPayload surface = traceTriangles(origin, direction, hasFloor ? floorDistance : sceneRange);
    if (surface.Distance == 0 && hasFloor) {
        surface.Normal = float3(0, 1, 0);
        surface.Distance = floorDistance;
        surface.Instance = floorMaterial;
    }
    return surface;
}
