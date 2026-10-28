#include "Camera.hlsl"
#include "Shading.hlsl"

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> OutputImage : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    uint width, height;
    OutputImage.GetDimensions(width, height);
    if (thread.x >= width || thread.y >= height)
        return;

    RayDesc ray = CameraRay(thread.xy, uint2(width, height));
    RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
    query.TraceRayInline(Scene, RAY_FLAG_NONE, 0xff, ray);
    // opaque triangles commit without shader callbacks
    while (query.Proceed()) {}

    float3 color = Sky(ray.Direction);
    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        color = ShadeHit(query.CommittedPrimitiveIndex(),
            query.CommittedTriangleBarycentrics(),
            (float3x3)query.CommittedWorldToObject3x4(),
            InstanceColor(query.CommittedInstanceID()));
    OutputImage[thread.xy] = float4(sqrt(color), 1.0);
}
