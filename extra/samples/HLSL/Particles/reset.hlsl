
struct Particle {
    float2 position;
    float2 velocity;
};

RWByteAddressBuffer particles : register(u0);

cbuffer Parameters : register(b0) {
    uint particleCount;
};

float3 hash(uint3 value)
{
    const uint multiplier = 1103515245u;
    value = ((value >> 8u) ^ value.yzx) * multiplier;
    value = ((value >> 8u) ^ value.yzx) * multiplier;
    value = ((value >> 8u) ^ value.yzx) * multiplier;
    return float3(value) / float(0xffffffffu);
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    uint id = dispatchId.x;
    if (id >= particleCount)
        return;

    Particle particle;
    particle.position = hash(uint3(id, 0, 1)).xy - 0.5;
    particle.velocity = 0.0;
    particles.Store2(id * 16, asuint(particle.position));
    particles.Store2(id * 16 + 8, asuint(particle.velocity));
}
