#version 330

uniform sampler1D uAudioSamples;

out vec4 oColor;

float lineCoverage(float y, float lineY, float pixelWidth) {
  return 1.0 - smoothstep(pixelWidth, pixelWidth * 2.5,
                          abs(y - lineY));
}

void main() {
  const vec2 resolution = vec2(960.0, 540.0);
  vec2 uv = gl_FragCoord.xy / resolution;
  vec2 samples = texture(uAudioSamples, clamp(uv.x, 0.0, 1.0)).rg;

  const float leftCenter = 0.72;
  const float rightCenter = 0.28;
  const float amplitude = 0.20;
  float pixelWidth = 1.0 / resolution.y;

  float leftLine = lineCoverage(
      uv.y, leftCenter + samples.r * amplitude, pixelWidth);
  float rightLine = lineCoverage(
      uv.y, rightCenter + samples.g * amplitude, pixelWidth);
  float centerLines = max(
      lineCoverage(uv.y, leftCenter, pixelWidth * 0.6),
      lineCoverage(uv.y, rightCenter, pixelWidth * 0.6));

  float verticalGrid = 1.0 - smoothstep(0.0, 0.002,
      abs(fract(uv.x * 10.0) - 0.5));
  float horizontalGrid = 1.0 - smoothstep(0.0, 0.004,
      abs(fract(uv.y * 10.0) - 0.5));
  float grid = max(verticalGrid, horizontalGrid);

  vec3 color = vec3(0.012, 0.018, 0.035);
  color += grid * vec3(0.018, 0.026, 0.048);
  color = mix(color, vec3(0.12, 0.18, 0.25), centerLines * 0.65);
  color = mix(color, vec3(0.10, 0.88, 1.00), leftLine);
  color = mix(color, vec3(1.00, 0.22, 0.68), rightLine);
  oColor = vec4(color, 1.0);
}
