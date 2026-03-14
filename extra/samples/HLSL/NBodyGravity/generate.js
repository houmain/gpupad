
app.loadLibrary("gl-matrix.js")
const vec3 = glMatrix.vec3
const vec4 = glMatrix.vec4
const mat4 = glMatrix.mat4

function RandomPercent() {
    const ret = (Math.random() * 10000) - 5000;
    return ret / 5000.0;
}

function LoadParticles(particles, offset, center, velocity, spread, numParticles) {
    for (let i = 0; i < numParticles; i++) {
        const delta = vec3.create();
        do {
            delta[0] = RandomPercent();
            delta[1] = RandomPercent();
            delta[1] = RandomPercent();
        } while (vec3.squaredLength(delta) > 1);
        
        particles[offset++] = center[0] + delta[0] * spread;
        particles[offset++] = center[1] + delta[1] * spread;
        particles[offset++] = center[2] + delta[2] * spread;
        particles[offset++] = 10000.0 * 10000.0,
        particles[offset++] = velocity[0];
        particles[offset++] = velocity[1];
        particles[offset++] = velocity[2];
        particles[offset++] = velocity[3];
    }
}

// Disk Galaxy Formation
const spread = 400.0
const data = []
LoadParticles(data, 0,
    vec3.fromValues(spread * 0.5, 0, 0),
    vec4.fromValues(0, 0, -20, 1 / 100000000),
    spread, ParticleCount / 2);
    
LoadParticles(data, ParticleCount / 2 * 8,
    vec3.fromValues(spread * 0.5, 0, 0),
    vec4.fromValues(0, 0, -20, 1 / 100000000),
    spread, ParticleCount / 2);
    
app.session.setBufferData("BufferR", data);
app.session.setBufferData("BufferW", data);

const colors = []
for (let i = 0, offset = 0; i < ParticleCount; i++) {
    colors[offset++] = 1.0;
    colors[offset++] = 1.0;
    colors[offset++] = 0.2;
    colors[offset++] = 1.0;
}
app.session.setBufferData("Colors", colors);
