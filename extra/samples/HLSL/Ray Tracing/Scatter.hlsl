
#include "Random.hlsl"
#include "RayPayload.hlsl"

float Schlick(float cosine, float refractionIndex)
{
    float r0 = (1.0 - refractionIndex) / (1.0 + refractionIndex);
    r0 *= r0;
    return r0 + (1.0 - r0) * pow(1.0 - cosine, 5.0);
}

RayPayload MakeRayPayload(
    float4 colorAndDistance, float4 scatterDirection, uint randomSeed)
{
    RayPayload payload;
    payload.ColorAndDistance = colorAndDistance;
    payload.ScatterDirection = scatterDirection;
    payload.RandomSeed = randomSeed;
    return payload;
}

float4 GetTextureColor(Material material, float2 texCoord)
{
    if (material.DiffuseTextureId >= 0)
        return DiffuseTexture.SampleLevel(
            _DiffuseTexture_sampler, texCoord, 0.0);
    return 1.0;
}

RayPayload ScatterLambertian(Material material, float3 direction,
    float3 normal, float2 texCoord, float distance, inout uint seed)
{
    bool isScattered = dot(direction, normal) < 0.0;
    float4 textureColor = GetTextureColor(material, texCoord);
    return MakeRayPayload(
        float4(material.Diffuse.rgb * textureColor.rgb, distance),
        float4(normal + RandomInUnitSphere(seed), isScattered ? 1.0 : 0.0),
        seed);
}

RayPayload ScatterMetallic(Material material, float3 direction,
    float3 normal, float2 texCoord, float distance, inout uint seed)
{
    float3 reflected = reflect(direction, normal);
    bool isScattered = dot(reflected, normal) > 0.0;
    float4 textureColor = GetTextureColor(material, texCoord);
    return MakeRayPayload(
        float4(material.Diffuse.rgb * textureColor.rgb, distance),
        float4(reflected + material.Fuzziness * RandomInUnitSphere(seed),
            isScattered ? 1.0 : 0.0),
        seed);
}

RayPayload ScatterDielectric(Material material, float3 direction,
    float3 normal, float2 texCoord, float distance, inout uint seed)
{
    float directionDotNormal = dot(direction, normal);
    float3 outwardNormal = directionDotNormal > 0.0 ? -normal : normal;
    float ratio = directionDotNormal > 0.0
        ? material.RefractionIndex
        : 1.0 / material.RefractionIndex;
    float cosine = directionDotNormal > 0.0
        ? material.RefractionIndex * directionDotNormal
        : -directionDotNormal;

    float3 refracted = refract(direction, outwardNormal, ratio);
    float reflectProbability = dot(refracted, refracted) > 0.0
        ? Schlick(cosine, material.RefractionIndex)
        : 1.0;
    float4 textureColor = GetTextureColor(material, texCoord);
    float3 scatterDirection = RandomFloat(seed) < reflectProbability
        ? reflect(direction, normal)
        : refracted;
    return MakeRayPayload(float4(textureColor.rgb, distance),
        float4(scatterDirection, 1.0), seed);
}

RayPayload ScatterDiffuseLight(
    Material material, float distance, inout uint seed)
{
    return MakeRayPayload(
        float4(material.Diffuse.rgb, distance), float4(1.0, 0.0, 0.0, 0.0),
        seed);
}

RayPayload Scatter(Material material, float3 direction, float3 normal,
    float2 texCoord, float distance, inout uint seed)
{
    float3 normalizedDirection = normalize(direction);
    switch (material.MaterialModel) {
    case MaterialLambertian:
        return ScatterLambertian(material, normalizedDirection, normal,
            texCoord, distance, seed);
    case MaterialMetallic:
        return ScatterMetallic(material, normalizedDirection, normal,
            texCoord, distance, seed);
    case MaterialDielectric:
        return ScatterDielectric(material, normalizedDirection, normal,
            texCoord, distance, seed);
    case MaterialDiffuseLight:
        return ScatterDiffuseLight(material, distance, seed);
    default:
        return MakeRayPayload(0.0, 0.0, seed);
    }
}
