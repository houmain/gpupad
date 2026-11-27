#include "../shared/StudioPayload.hlsl"
#include "../shared/StudioLighting.hlsl"

static const float3 cameraOrigin = float3(2.8, 2.65, -7.2);
static const float3 lightPosition = float3(-3.1, 4.6, -2.6);

float3 cameraDirection(uint2 pixel, uint2 size)
{
    float2 uv = (float2(pixel) + 0.5) / float2(size) * 2 - 1;
    uv *= float2(float(size.x) / float(size.y), -1);
    float3 forward = normalize(float3(0, 0.95, 0.7) - cameraOrigin);
    float3 right = normalize(cross(float3(0, 1, 0), forward));
    float3 up = cross(forward, right);
    return normalize(forward + 0.32 * (uv.x * right + uv.y * up));
}

float3 studio(float3 direction)
{
    float key = studioLobe(direction, float3(-2, 3, -1), 16);
    float rim = studioLobe(direction, float3(2, 1, 2), 32);
    float panel = studioPanel(direction, 0.17, 9, -0.04, 0.50);
    return lerp(float3(0.025, 0.038, 0.064), float3(0.10, 0.15, 0.22),
        saturate(0.4 + direction.y)) + key * float3(4.5, 3.8, 3.1)
        + rim * float3(0.6, 1.4, 2.5) + panel * float3(1.8, 2.0, 2.2);
}

float3 baseColor(uint material, float3 position)
{
    if (material == 0)
        return float3(0.96, 0.58, 0.26);
    if (material == 1)
        return float3(0.78, 0.85, 0.97);
    if (material == 2)
        return float3(0.21, 0.68, 0.74);

    float tileEdge = max(abs(frac(position.x * 0.5 + 0.5) - 0.5),
        abs(frac(position.z * 0.5 + 0.5) - 0.5));
    float fade = saturate(1 - length(position.xz) * 0.075);
    return float3(0.15, 0.20, 0.27) * (1 - 0.17 * fade * smoothstep(0.488, 0.499, tileEdge));
}

float3 fillLighting(float3 albedo, float3 normal)
{
    return albedo * (0.07 + 0.14 * studio(normal));
}

float3 keyLighting(float3 albedo, float3 position, float3 normal)
{
    float3 toLight = lightPosition - position;
    float distanceSquared = dot(toLight, toLight);
    float cosine = saturate(dot(normal, toLight * rsqrt(max(distanceSquared, 1e-6))));
    return albedo * float3(2.5, 2.1, 1.8) * cosine / (1 + 0.052 * distanceSquared);
}
