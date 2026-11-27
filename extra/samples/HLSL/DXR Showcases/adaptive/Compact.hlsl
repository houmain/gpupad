StructuredBuffer<uint> Eligibility : register(t0);
RWStructuredBuffer<uint> PixelQueue : register(u0);
RWStructuredBuffer<uint> ActiveCount : register(u1);

[numthreads(64, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    uint capacity, stride;
    Eligibility.GetDimensions(capacity, stride);
    uint index = thread.x;
    bool selected = false;
    if (index < capacity)
        selected = Eligibility[index] != 0;

    uint laneOffset = WavePrefixCountBits(selected);
    uint waveCount = WaveActiveCountBits(selected);
    if (waveCount == 0)
        return;

    uint waveOffset = 0;
    if (WaveIsFirstLane())
        InterlockedAdd(ActiveCount[0], waveCount, waveOffset);
    waveOffset = WaveReadLaneFirst(waveOffset);
    if (selected)
        PixelQueue[waveOffset + laneOffset] = index;
}
