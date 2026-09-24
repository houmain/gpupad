
#include "Random.hlsl"
#include "RayPayload.hlsl"
#include "Camera.hlsl"

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> AccumulationImage : register(u0);
RWTexture2D<float4> OutputImage : register(u1);

[shader("raygeneration")]
void RayGeneration()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDimensions = DispatchRaysDimensions().xy;

    uint pixelRandomSeed = RandomSeed;
    RayPayload ray;
    ray.RandomSeed = InitRandomSeed(
        InitRandomSeed(launchIndex.x, launchIndex.y), TotalNumberOfSamples);

    float3 pixelColor = 0.0;
    for (uint sample = 0; sample < NumberOfSamples; ++sample) {
        float2 pixel = float2(launchIndex)
            + float2(RandomFloat(pixelRandomSeed),
                RandomFloat(pixelRandomSeed));
        float2 uv = pixel / float2(launchDimensions) * 2.0 - 1.0;

        float2 offset = Aperture * 0.5 * RandomInUnitDisk(ray.RandomSeed);
        float4 origin = mul(float4(offset, 0.0, 1.0), ModelViewInverse);
        float4 target = mul(float4(uv, 1.0, 1.0), ProjectionInverse);
        float4 direction = mul(float4(normalize(
            target.xyz * FocusDistance - float3(offset, 0.0)), 0.0),
            ModelViewInverse);
        float3 rayColor = 1.0;

        for (uint bounce = 0; bounce <= NumberOfBounces; ++bounce) {
            if (bounce == NumberOfBounces) {
                rayColor = 0.0;
                break;
            }

            RayDesc rayDescription;
            rayDescription.Origin = origin.xyz;
            rayDescription.TMin = 0.001;
            rayDescription.Direction = direction.xyz;
            rayDescription.TMax = 10000.0;
            TraceRay(Scene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0,
                rayDescription, ray);

            float3 hitColor = ray.ColorAndDistance.rgb;
            float distance = ray.ColorAndDistance.w;
            bool isScattered = ray.ScatterDirection.w > 0.0;
            rayColor *= hitColor;
            if (distance < 0.0 || !isScattered)
                break;

            origin += distance * direction;
            direction = float4(ray.ScatterDirection.xyz, 0.0);
        }
        pixelColor += rayColor;
    }

    bool accumulate = NumberOfSamples != TotalNumberOfSamples;
    float3 accumulatedColor =
        (accumulate ? AccumulationImage[launchIndex].rgb : 0.0) + pixelColor;
    pixelColor = accumulatedColor / TotalNumberOfSamples;
    pixelColor = sqrt(pixelColor);

    AccumulationImage[launchIndex] = float4(accumulatedColor, 1.0);
    OutputImage[launchIndex] = float4(pixelColor, 1.0);
}
