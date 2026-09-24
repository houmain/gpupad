
#include "RayPayload.hlsl"
#include "Camera.hlsl"

[shader("miss")]
void RayMiss(inout RayPayload ray)
{
    if (HasSky != 0) {
        float blend = 0.5 * (normalize(WorldRayDirection()).y + 1.0);
        float3 skyColor = lerp(1.0, float3(0.5, 0.7, 1.0), blend);
        ray.ColorAndDistance = float4(skyColor, -1.0);
    } else {
        ray.ColorAndDistance = float4(0.0, 0.0, 0.0, -1.0);
    }
}
