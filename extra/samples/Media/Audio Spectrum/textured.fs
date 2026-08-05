#version 330

uniform sampler2D uSpectrum;
uniform sampler2D uHistory;

out vec4 oColor;

vec3 heatMap(float value) {
  value = clamp(value, 0.0, 1.0);

  const vec3 blackPurple = vec3(0.008, 0.000, 0.055);
  const vec3 violet     = vec3(0.220, 0.000, 0.430);
  const vec3 magenta    = vec3(0.690, 0.055, 0.455);
  const vec3 orange     = vec3(1.000, 0.310, 0.055);
  const vec3 yellow     = vec3(1.000, 0.850, 0.180);

  if (value < 0.25)
    return mix(blackPurple, violet, smoothstep(0.0, 0.25, value));
  if (value < 0.50)
    return mix(violet, magenta, smoothstep(0.25, 0.50, value));
  if (value < 0.75)
    return mix(magenta, orange, smoothstep(0.50, 0.75, value));
  return mix(orange, yellow, smoothstep(0.75, 1.0, value));
}

void main() {
  ivec2 pixel = ivec2(gl_FragCoord.xy);
  ivec2 historySize = textureSize(uHistory, 0);

  // Copy from the pixel to the right so the accumulated history travels left.
  if (pixel.x < historySize.x - 1) {
    oColor = texelFetch(uHistory, pixel + ivec2(1, 0), 0);
    return;
  }

  // Spread the linear FFT bins over a logarithmic frequency axis. The first
  // useful bin is at the bottom and Nyquist is at the top.
  ivec2 spectrumSize = textureSize(uSpectrum, 0);
  float heightPosition = (float(pixel.y) + 0.5) / float(historySize.y);
  float firstBin = 1.0;
  float lastBin = float(spectrumSize.x - 1);
  float bin = exp2(mix(log2(firstBin), log2(lastBin), heightPosition));
  float spectrumU = (bin + 0.5) / float(spectrumSize.x);
  float amplitude = max(texture(uSpectrum, vec2(spectrumU, 0.5)).r, 1e-7);

  // Convert amplitude to decibels and slightly compensate high frequencies.
  float decibels = 20.0 * log(amplitude) / log(10.0);
  float level = clamp((decibels + 82.0 + 5.0 * heightPosition) / 62.0,
                      0.0, 1.0);
  level = pow(level, 0.8);

  oColor = vec4(heatMap(level), 1.0);
}
