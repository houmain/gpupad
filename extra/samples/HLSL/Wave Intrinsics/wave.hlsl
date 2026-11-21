StructuredBuffer<uint> Inputs : register(t0);
RWStructuredBuffer<uint> Results : register(u0);

[numthreads(64, 1, 1)]
void main(uint id : SV_GroupIndex)
{
    if (id == 0) {
        Results[0] = 0;
        Results[1] = 0;
    }
    Results[2 + id] = 0xffffffff;
    AllMemoryBarrierWithGroupSync();

    uint sum = WaveActiveSum(Inputs[id]);
    if (WaveIsFirstLane())
        InterlockedAdd(Results[0], sum);

    bool keep = (id % 2 == 0);
    uint index = WavePrefixCountBits(keep);
    uint count = WaveActiveCountBits(keep);
    uint offset = 0;
    if (WaveIsFirstLane())
        InterlockedAdd(Results[1], count, offset);
    offset = WaveReadLaneFirst(offset);

    if (keep)
        Results[2 + offset + index] = id;
}
