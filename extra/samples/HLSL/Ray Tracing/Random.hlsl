
uint InitRandomSeed(uint value0, uint value1)
{
    uint v0 = value0;
    uint v1 = value1;
    uint sum = 0;

    [unroll]
    for (uint n = 0; n < 16; ++n) {
        sum += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + sum)
            ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + sum)
            ^ ((v0 >> 5) + 0x7e95761e);
    }
    return v0;
}

uint RandomInt(inout uint seed)
{
    seed = 1664525 * seed + 1013904223;
    return seed;
}

float RandomFloat(inout uint seed)
{
    return float(RandomInt(seed) & 0x00ffffff) / float(0x01000000);
}

float2 RandomInUnitDisk(inout uint seed)
{
    for (;;) {
        float2 samplePoint = 2.0 * float2(
            RandomFloat(seed), RandomFloat(seed)) - 1.0;
        if (dot(samplePoint, samplePoint) < 1.0)
            return samplePoint;
    }
}

float3 RandomInUnitSphere(inout uint seed)
{
    for (;;) {
        float3 samplePoint = 2.0 * float3(RandomFloat(seed),
            RandomFloat(seed), RandomFloat(seed)) - 1.0;
        if (dot(samplePoint, samplePoint) < 1.0)
            return samplePoint;
    }
}
