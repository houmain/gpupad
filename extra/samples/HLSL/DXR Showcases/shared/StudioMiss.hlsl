#include "StudioPayload.hlsl"

[shader("miss")]
void RayMiss(inout RayPayload payload)
{
    payload.Distance = 0;
    payload.Instance = invalidInstance;
}
