#include "RayPayload.hlsl"

[shader("miss")]
void RayMiss(inout RayPayload payload)
{
    float t = 0.5 * (normalize(WorldRayDirection()).y + 1.0);
    payload.Color = lerp(float3(0.04, 0.05, 0.08),
        float3(0.18, 0.22, 0.32), t);
}
