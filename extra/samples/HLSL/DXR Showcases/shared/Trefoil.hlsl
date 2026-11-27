static const uint vertexStride = 24;

float3 vertexNormal(ByteAddressBuffer vertices, ByteAddressBuffer indices, uint index)
{
    uint vertex = indices.Load(4 * index);
    return asfloat(vertices.Load3(vertex * vertexStride + 12));
}

float3 trefoilNormal(ByteAddressBuffer vertices, ByteAddressBuffer indices, uint primitive,
    float2 bary, float3x3 worldToObject, float3 rayDirection)
{
    uint i = 3 * primitive;
    // weights for vertices 1 and 2 (dxr builtintriangleintersectionattributes)
    float3 normal = vertexNormal(vertices, indices, i) * (1 - bary.x - bary.y)
        + vertexNormal(vertices, indices, i + 1) * bary.x
        + vertexNormal(vertices, indices, i + 2) * bary.y;
    normal = normalize(mul(normal, worldToObject));
    return dot(normal, rayDirection) > 0 ? -normal : normal;
}
