#include "RayPayload.hlsl"
#include "Shading.hlsl"

[shader("closesthit")]
void RayClosestHit(inout RayPayload payload,
    BuiltInTriangleIntersectionAttributes attributes)
{
    payload.Color = ShadeHit(PrimitiveIndex(), attributes.barycentrics,
        (float3x3)WorldToObject3x4(), InstanceColor(InstanceID()));
}
