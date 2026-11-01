RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> OutputImage : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint width;
    uint height;
    OutputImage.GetDimensions(width, height);
    if (dispatchThreadId.x >= width || dispatchThreadId.y >= height)
        return;

    const float2 uv =
        (float2(dispatchThreadId.xy) + 0.5) / float2(width, height) * 2.0 - 1.0;
    RayDesc ray;
    ray.Origin = float3(0.0, 0.0, 0.0);
    ray.Direction = normalize(float3(uv, 1.0));
    ray.TMin = 0.001;
    ray.TMax = 100.0;

    RayQuery<RAY_FLAG_CULL_NON_OPAQUE
        | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
    query.TraceRayInline(Scene, RAY_FLAG_NONE, 0xff, ray);
    while (query.Proceed()) {
    }

    const bool hit = query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
    OutputImage[dispatchThreadId.xy] =
        (hit ? float4(1.0, 0.55, 0.1, 1.0)
             : float4(0.02, 0.04, 0.08, 1.0));
}
