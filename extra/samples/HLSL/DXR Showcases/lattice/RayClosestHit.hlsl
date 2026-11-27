#include "RayPayload.hlsl"
#include "../shared/Trefoil.hlsl"

ByteAddressBuffer Vertices : register(t1);
ByteAddressBuffer Indices : register(t2);
StructuredBuffer<uint> MaterialIndices : register(t3);
StructuredBuffer<float4> Materials[] : register(t0, space1);

cbuffer HitRecordData : register(b1)
{
    uint recordStamp;
};

[shader("closesthit")]
void RayClosestHit(inout RayPayload payload,
    BuiltInTriangleIntersectionAttributes attributes)
{
    // dxr instanceid indexes material data independently of the hit group
    uint instance = InstanceID();
    // lane-varying descriptor index (rtgbook, #2, section 17.3)
    uint slot = NonUniformResourceIndex(MaterialIndices[instance]);
    float4 material = Materials[slot][0];
    uint materialStamp = uint(Materials[slot][1].x);
    float3 normal = trefoilNormal(Vertices, Indices, PrimitiveIndex(), attributes.barycentrics,
        (float3x3)WorldToObject3x4(), WorldRayDirection());

    float recordGain = 0.7 + 0.012 * recordStamp;
    payload.Radiance = recordGain * material.rgb * (0.14 + 0.50 * studio(normal));
    payload.Normal = normal;
    payload.Reflectance = material.rgb * material.a;
    payload.Distance = RayTCurrent();
    payload.Stamp = uint3(instance, recordStamp, materialStamp);
}
