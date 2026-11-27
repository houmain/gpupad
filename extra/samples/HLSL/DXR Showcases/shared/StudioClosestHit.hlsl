#include "StudioPayload.hlsl"
#include "Trefoil.hlsl"

ByteAddressBuffer Vertices : register(t1);
ByteAddressBuffer Indices : register(t2);

[shader("closesthit")]
void RayClosestHit(inout RayPayload payload, BuiltInTriangleIntersectionAttributes attributes)
{
    payload.Normal = trefoilNormal(Vertices, Indices, PrimitiveIndex(), attributes.barycentrics,
        (float3x3)WorldToObject3x4(), WorldRayDirection());
    payload.Distance = RayTCurrent();
    payload.Instance = InstanceID();
}
