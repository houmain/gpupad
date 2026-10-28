#include "Camera.hlsl"
#include "RayPayload.hlsl"

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> OutputImage : register(u0);

[shader("raygeneration")]
void RayGeneration()
{
    uint2 pixel = DispatchRaysIndex().xy;
    RayDesc ray = CameraRay(pixel, DispatchRaysDimensions().xy);
    RayPayload payload;
    payload.Color = 0.0;
    TraceRay(Scene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray, payload);
    OutputImage[pixel] = float4(sqrt(payload.Color), 1.0);
}
