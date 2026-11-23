#include "../shared/Trefoil.hlsl"

ByteAddressBuffer RestVertices : register(t0);
RWByteAddressBuffer Vertices : register(u0);

cbuffer Deformation : register(b0)
{
    uint vertexCount;
    float twist;
};

float3 rotateY(float3 p, float sine, float cosine)
{
    return float3(cosine * p.x + sine * p.z, p.y, -sine * p.x + cosine * p.z);
}

[numthreads(64, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    if (thread.x >= vertexCount)
        return;

    uint offset = thread.x * vertexStride;
    float3 position = asfloat(RestVertices.Load3(offset));
    float3 normal = asfloat(RestVertices.Load3(offset + 12));
    float sine, cosine;
    sincos(twist * position.y, sine, cosine);
    position = rotateY(position, sine, cosine);
    normal = rotateY(normal, sine, cosine);

    normal.y -= twist * (position.z * normal.x - position.x * normal.z);
    // positions feed d3d12_raytracing_geometry_triangles_desc.vertexbuffer
    Vertices.Store3(offset, asuint(position));
    Vertices.Store3(offset + 12, asuint(normalize(normal)));
}
