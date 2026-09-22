
RWStructuredBuffer<uint> uIndirectBuffer : register(u0);

cbuffer Dimensions : register(b0) {
    int uCountX;
    int uCountY;
};

[numthreads(1, 1, 1)]
void main() {
    uIndirectBuffer[0] = uint(max(0, uCountX + 15) / 16);
    uIndirectBuffer[1] = uint(max(0, uCountY + 15) / 16);
    uIndirectBuffer[2] = 1;
    uIndirectBuffer[3] = uint(max(0, (uCountX * uCountY) * 6));
    uIndirectBuffer[4] = 1;
    uIndirectBuffer[5] = 0;
    uIndirectBuffer[6] = 0;
}
