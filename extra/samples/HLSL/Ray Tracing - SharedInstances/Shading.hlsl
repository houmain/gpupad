ByteAddressBuffer Vertices : register(t1);
ByteAddressBuffer Indices : register(t2);

#include "Vertex.hlsl"

float3 InstanceColor(uint index)
{
    static const float3 colors[3] = {
        float3(0.8, 0.2, 0.12), float3(0.15, 0.65, 0.25),
        float3(0.15, 0.35, 0.85)
    };
    return colors[index % 3];
}

float3 ShadeHit(uint primitive, float2 uv, float3x3 worldToObject,
    float3 color)
{
    uint triangleIndex = primitive * 3;
    Vertex a = UnpackVertex(Indices.Load(triangleIndex * 4));
    Vertex b = UnpackVertex(Indices.Load((triangleIndex + 1) * 4));
    Vertex c = UnpackVertex(Indices.Load((triangleIndex + 2) * 4));
    float3 normal = a.Normal * (1.0 - uv.x - uv.y)
        + b.Normal * uv.x + c.Normal * uv.y;
    // transform normals with the inverse transpose
    normal = normalize(mul(normal, worldToObject));
    float diffuse = saturate(dot(normal, normalize(float3(1.0, 2.0, 1.0))));
    return color * (0.15 + 0.85 * diffuse);
}

float3 Sky(float3 direction)
{
    return lerp(float3(0.8, 0.85, 0.9), float3(0.3, 0.5, 0.8),
        saturate(direction.y * 0.5 + 0.5));
}
