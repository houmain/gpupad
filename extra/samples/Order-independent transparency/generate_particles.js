
function random(min, max) {
  return Math.random() * (max - min) + min
}

let particles = new Array(ParticlesCount * 12)
for (let i = 0, p = 0; i < ParticlesCount; ++i) {
  // position
  particles[p++] = random(-16, 16)
  particles[p++] = random(-16, 16)
  particles[p++] = random(-16, 16)
  
  // size
  particles[p++] = 3
  
  // velocity
  particles[p++] = random(0, 0.01)
  particles[p++] = random(0, 0.01)
  particles[p++] = random(0, 0.01)
  particles[p++] = 0
  
  // color
  particles[p++] = random(0, 1)
  particles[p++] = random(0, 1)
  particles[p++] = random(0, 1)
  particles[p++] = 0.4
}

Session.setBufferData(
  Session.item('Buffers/Particle Buffer'), particles)
