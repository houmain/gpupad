#include "../shared/StudioLighting.hlsl"

struct RayPayload
{
    float3 Radiance;
    float3 Normal;
    float3 Reflectance;
    float Distance;
    uint3 Stamp;
};

float3 studio(float3 direction)
{
    float key = studioLobe(direction, float3(-2, 3, -1), 14);
    float rim = studioLobe(direction, float3(2, -1, 2), 40);
    float strip = studioPanel(direction, 0.16, 8, -0.1, 0.65);
    return float3(0.035, 0.053, 0.093)
        + key * float3(4.2, 3.4, 2.9)
        + rim * float3(0.50, 1.10, 2.30)
        + strip * float3(1.65, 1.85, 2.00);
}
