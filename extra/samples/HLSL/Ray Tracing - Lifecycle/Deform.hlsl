ByteAddressBuffer RestVertices : register(t0);
RWByteAddressBuffer Vertices : register(u0);

cbuffer Deformation : register(b0) {
    uint VertexCount;
    float Time;
};

[numthreads(64, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    if (thread.x >= VertexCount)
        return;
    uint offset = thread.x * 32;
    float3 position = asfloat(RestVertices.Load3(offset));
    float3 normal = asfloat(RestVertices.Load3(offset + 12));
    float scale = 1.0 + 0.25 * sin(Time);
    float shear = 0.3 * sin(Time * 1.3);
    position.x += shear * position.y;
    position.y *= scale;
    // apply the inverse transpose of the same deformation
    normal.y = (normal.y - shear * normal.x) / scale;
    Vertices.Store3(offset, asuint(position));
    Vertices.Store3(offset + 12, asuint(normalize(normal)));
}
