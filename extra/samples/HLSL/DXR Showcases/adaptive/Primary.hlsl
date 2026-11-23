#include "Scene.hlsl"
#include "../shared/StudioTrace.hlsl"

RWTexture2D<float4> SurfacePosition : register(u0);
RWTexture2D<float4> SurfaceNormal : register(u1);

cbuffer Settings : register(b0)
{
    uint fixture;
};

[shader("raygeneration")]
void RayGeneration()
{
    uint2 pixel = DispatchRaysIndex().xy;
    float3 origin, direction;
    cameraRay(pixel, DispatchRaysDimensions().xy, fixture != 0, origin, direction);
    RayPayload surface = traceSurface(origin, direction);
    if (surface.Distance > 0) {
        float3 position = origin + surface.Distance * direction;
        SurfacePosition[pixel] = float4(position, float(surface.Instance + 1));
        SurfaceNormal[pixel] = float4(
            surface.Normal, polish(surface.Instance, position, fixture != 0));
    } else {
        SurfacePosition[pixel] = 0;
        SurfaceNormal[pixel] = 0;
    }
}
