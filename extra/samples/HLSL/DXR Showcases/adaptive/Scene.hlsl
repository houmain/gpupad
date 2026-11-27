#include "../shared/StudioPayload.hlsl"
#include "../shared/StudioLighting.hlsl"

void cameraRay(uint2 pixel, uint2 size, bool fixture, out float3 origin, out float3 direction)
{
    float2 uv = (float2(pixel) + 0.5) / float2(size);
    if (fixture) {
        origin = float3(-3 + 6 * uv.x, 0.045, -2 + 4 * uv.y);
        direction = float3(0, -0.8, 0.6);
        return;
    }

    origin = float3(2.45, 2.60, -7.15);
    float3 forward = normalize(float3(0, 0.84, 0.7) - origin);
    float3 right = normalize(cross(float3(0, 1, 0), forward));
    float3 up = cross(forward, right);
    uv = (2 * uv - 1) * float2(float(size.x) / float(size.y), -1);
    direction = normalize(forward + 0.34 * (uv.x * right + uv.y * up));
}

float3 studio(float3 direction)
{
    float key = studioLobe(direction, float3(-2, 3, -1), 16);
    float rim = studioLobe(direction, float3(2, 1, 2), 30);
    float panel = studioPanel(direction, 0.17, 9, -0.05, 0.51);
    return lerp(float3(0.025, 0.036, 0.064), float3(0.10, 0.15, 0.22),
        saturate(0.4 + direction.y)) + key * float3(4.5, 3.8, 3.1)
        + rim * float3(0.6, 1.5, 2.8) + panel * float3(1.9, 2.0, 2.2);
}

float polish(uint material, float3 position, bool fixture)
{
    if (material == 0)
        return 0.75;
    if (material == 1)
        return 0.12;
    if (material == 2)
        return 0.82;
    if (fixture)
        return 0.6;

    float band = frac(position.x * 0.38 + position.z * 0.14 + 0.16);
    return band < 0.37 ? 0.56 : 0.07;
}

float3 materialColor(uint material, float reflectivity)
{
    if (material == 0)
        return float3(0.96, 0.52, 0.23);
    if (material == 1)
        return float3(0.78, 0.86, 0.94);
    if (material == 2)
        return float3(0.20, 0.66, 0.74);
    return reflectivity >= 0.4 ? float3(0.10, 0.15, 0.20) : float3(0.15, 0.19, 0.24);
}

float3 surfaceLighting(uint material, float reflectivity, float3 position, float3 normal)
{
    float3 toLight = float3(-3.1, 4.6, -2.6) - position;
    float distanceSquared = dot(toLight, toLight);
    float cosine = saturate(dot(normal, toLight * rsqrt(max(distanceSquared, 1e-6))));
    float3 key = float3(1.75, 1.55, 1.38) * cosine / (1 + 0.055 * distanceSquared);
    return materialColor(material, reflectivity) * (0.07 + 0.14 * studio(normal) + key);
}
