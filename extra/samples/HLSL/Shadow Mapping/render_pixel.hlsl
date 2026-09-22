
Texture2D<float4> uAlbedo : register(t0);
Texture2D<float4> uNormal : register(t1);
Texture2D<float4> uAmbientOcclusion : register(t2);
Texture2D<float4> uMetalRoughness : register(t3);
Texture2D<float4> uEmissive : register(t4);
Texture2D<float> uShadowMap : register(t5);
SamplerState materialSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

cbuffer Transforms : register(b0) {
    row_major float4x4 uModel;
    row_major float4x4 uView;
    row_major float4x4 uProjection;
    row_major float4x4 uLightView;
    row_major float4x4 uLightProjection;
};

struct FragmentInput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    float4 shadowCoord : TEXCOORD1;
};

float3x3 cotangentFrame(float3 normal, float3 position, float2 texCoord)
{
    float3 positionDx = ddx(position);
    float3 positionDy = ddy(position);
    float2 texCoordDx = ddx(texCoord);
    float2 texCoordDy = ddy(texCoord);
    float determinant = texCoordDx.x * texCoordDy.y - texCoordDy.x * texCoordDx.y;
    if (abs(determinant) < 1e-8)
        return float3x3(0.0.xxx, 0.0.xxx, normal);
    float3 tangent = normalize((texCoordDy.y * positionDx - texCoordDx.y * positionDy)
                               / determinant);
    tangent = normalize(tangent - normal * dot(normal, tangent));
    return float3x3(tangent, normalize(cross(normal, tangent)), normal);
}

float4 main(FragmentInput input) : SV_Target0
{
    static const float2 poissonDisk[4] = {
        float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725),
        float2(-0.09418410, -0.92938870), float2(0.34495938, 0.29387760)
    };

    float3 albedo = uAlbedo.Sample(materialSampler, input.texCoord).rgb;
    float3 mappedNormal = uNormal.Sample(materialSampler, input.texCoord).xyz * 2.0 - 1.0;
    float3 normal = normalize(mul(mappedNormal,
        cotangentFrame(normalize(input.normal), input.worldPosition, input.texCoord)));
    float3 viewDirection = normalize(float3(0.0, 0.0, 3.5) - input.worldPosition);
    float3 lightDirection = normalize(float3(uLightView[0][2], uLightView[1][2], uLightView[2][2]));
    float ambientOcclusion = uAmbientOcclusion.Sample(materialSampler, input.texCoord).r;
    float3 emissive = uEmissive.Sample(materialSampler, input.texCoord).rgb;
    float2 metalRoughness = uMetalRoughness.Sample(materialSampler, input.texCoord).bg;
    float diffuse = 0.9 * max(dot(normal, lightDirection), 0.0) * (1.0 - metalRoughness.x);
    float3 specularColor = lerp(0.04.xxx, albedo, metalRoughness.x);
    float cosine = saturate(dot(viewDirection, reflect(-lightDirection, normal)));
    float specular = pow(cosine, lerp(5.0, 2.0, metalRoughness.y * metalRoughness.y));

    float visibility = 1.0;
    float3 shadowCoord = input.shadowCoord.xyz / input.shadowCoord.w;
    for (uint i = 0; i < 4; ++i) {
        float shadow = uShadowMap.SampleCmpLevelZero(shadowSampler,
            shadowCoord.xy + poissonDisk[i] / 700.0, shadowCoord.z - 0.0025);
        visibility -= 0.2 * (1.0 - shadow);
    }

    float3 color = albedo * (0.2 * ambientOcclusion);
    color += visibility * (albedo * diffuse + specularColor * specular);
    color += emissive;
    return float4(color, 1.0);
}
