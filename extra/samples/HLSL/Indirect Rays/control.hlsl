RWStructuredBuffer<uint> ComputeResults : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    ComputeResults[id.x] = 200 + id.x;
}
