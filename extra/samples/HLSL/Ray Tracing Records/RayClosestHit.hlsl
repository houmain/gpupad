#include "RayPayload.hlsl"

#if defined(GPUPAD_VULKAN)
[[vk::shader_record_ext]]
cbuffer HitRecordData
#else
cbuffer HitRecordData : register(b1)
#endif
{
    float4 HitColor;
};

[shader("closesthit")]
void RayClosestHit(inout RayPayload payload,
    BuiltInTriangleIntersectionAttributes attributes)
{
    float edgeShade = 0.65 + 0.35 *
        (1.0 - attributes.barycentrics.x - attributes.barycentrics.y);
    payload.Color = HitColor.rgb * edgeShade;
}
