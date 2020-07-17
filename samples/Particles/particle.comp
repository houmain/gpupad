#version 430

struct Particle {
  vec4 position;
  vec4 velocity;
};

layout(std430, binding=0) buffer particles {
  Particle particle[];
};

uniform uint particleCount;
