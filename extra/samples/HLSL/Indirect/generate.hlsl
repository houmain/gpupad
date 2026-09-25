
RWStructuredBuffer<float2> uVertexBuffer : register(u0);

cbuffer Dimensions : register(b1) {
    int uCountX;
    int uCountY;
};

[numthreads(16, 16, 1)]
void main(uint3 globalId : SV_DispatchThreadID) {
    uint x = globalId.x;
    uint y = globalId.y;
    if (x >= uint(uCountX) || y >= uint(uCountY))
        return;

    uint offset = (y * uint(uCountX) + x) * 6;
    float sx = 2.0 / float(uCountX);
    float sy = 2.0 / float(uCountY);
    float2 v0 = float2((x + 0) * sx, (y + 0) * sy) - 1.0;
    float2 v1 = float2((x + 1) * sx, (y + 0) * sy) - 1.0;
    float2 v2 = float2((x + 0) * sx, (y + 1) * sy) - 1.0;
    float2 v3 = float2((x + 1) * sx, (y + 1) * sy) - 1.0;
    const float indent = 0.3;

    float2 mid = (v0 + v1 + v2) / 3.0;
    uVertexBuffer[offset + 0] = lerp(v0, mid, indent);
    uVertexBuffer[offset + 1] = lerp(v1, mid, indent);
    uVertexBuffer[offset + 2] = lerp(v2, mid, indent);

    mid = (v1 + v2 + v3) / 3.0;
    uVertexBuffer[offset + 4] = lerp(v1, mid, indent);
    uVertexBuffer[offset + 3] = lerp(v3, mid, indent);
    uVertexBuffer[offset + 5] = lerp(v2, mid, indent);
}
