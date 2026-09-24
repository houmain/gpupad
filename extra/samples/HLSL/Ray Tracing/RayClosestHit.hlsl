
#include "Material.hlsl"

ByteAddressBuffer Vertices : register(t1);
ByteAddressBuffer Indices : register(t2);
StructuredBuffer<Material> Materials : register(t3);
Texture2D<float4> DiffuseTexture : register(t4);
SamplerState _DiffuseTexture_sampler : register(s0);

#include "Scatter.hlsl"
#include "Vertex.hlsl"

float2 Interpolate(float2 a, float2 b, float2 c, float3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

float3 Interpolate(float3 a, float3 b, float3 c, float3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

[shader("closesthit")]
void RayClosestHit(
    inout RayPayload ray, BuiltInTriangleIntersectionAttributes attributes)
{
    uint triangleIndex = PrimitiveIndex() * 3;
    Vertex vertex0 = UnpackVertex(Indices.Load(triangleIndex * 4));
    Vertex vertex1 = UnpackVertex(Indices.Load((triangleIndex + 1) * 4));
    Vertex vertex2 = UnpackVertex(Indices.Load((triangleIndex + 2) * 4));
    Material material = Materials[InstanceID()];

    float3 barycentrics = float3(
        1.0 - attributes.barycentrics.x - attributes.barycentrics.y,
        attributes.barycentrics);
    float3 normal = normalize(Interpolate(vertex0.Normal, vertex1.Normal,
        vertex2.Normal, barycentrics));
    float2 texCoord = Interpolate(vertex0.TexCoord, vertex1.TexCoord,
        vertex2.TexCoord, barycentrics);

    ray = Scatter(material, WorldRayDirection(), normal, texCoord,
        RayTCurrent(), ray.RandomSeed);
}
