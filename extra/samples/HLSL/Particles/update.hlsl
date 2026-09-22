
struct Particle {
    float2 position;
    float2 velocity;
};

RWByteAddressBuffer particles : register(u0);

cbuffer Parameters : register(b0) {
    uint particleCount;
    float2 attractor;
};

[numthreads(256, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    uint id = dispatchId.x;
    if (id >= particleCount)
        return;

    Particle particle;
    particle.position = asfloat(particles.Load2(id * 16));
    particle.velocity = asfloat(particles.Load2(id * 16 + 8));
    float2 acceleration = normalize(attractor - particle.position) / 100.0;
    particle.velocity += acceleration;
    particle.position = clamp(particle.position + particle.velocity, -1.1, 1.1);
    particles.Store2(id * 16, asuint(particle.position));
    particles.Store2(id * 16 + 8, asuint(particle.velocity));
}
