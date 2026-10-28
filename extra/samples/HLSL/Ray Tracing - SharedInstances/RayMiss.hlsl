#include "RayPayload.hlsl"
#include "Shading.hlsl"

[shader("miss")]
void RayMiss(inout RayPayload payload)
{
    payload.Color = Sky(WorldRayDirection());
}
