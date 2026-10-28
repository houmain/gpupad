cbuffer Camera : register(b0) {
    row_major float4x4 ModelViewInverse;
    row_major float4x4 ProjectionInverse;
};

RayDesc CameraRay(uint2 pixel, uint2 size)
{
    float2 uv = (float2(pixel) + 0.5) / float2(size) * 2.0 - 1.0;
    float4 target = mul(float4(uv, 1.0, 1.0), ProjectionInverse);
    RayDesc ray;
    ray.Origin = mul(float4(0.0, 0.0, 0.0, 1.0), ModelViewInverse).xyz;
    ray.Direction = normalize(mul(float4(normalize(target.xyz), 0.0),
        ModelViewInverse).xyz);
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    return ray;
}
