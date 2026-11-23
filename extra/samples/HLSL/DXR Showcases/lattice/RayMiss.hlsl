#include "RayPayload.hlsl"

[shader("miss")]
void RayMiss(inout RayPayload payload)
{
    payload.Radiance = studio(WorldRayDirection());
    payload.Distance = 0;
    payload.Stamp = 0xffffffffu;
}
