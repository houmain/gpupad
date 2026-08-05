#version 330

uniform sampler2D uSpectrum;
uniform sampler2D uPreviousState;
uniform float uSampleRate;
uniform vec2 uDecibelRange;
uniform float uIntensityGamma;
uniform float uIntensityAttackRate;
uniform float uIntensityReleaseRate;
uniform float uBaselineRiseRate;
uniform float uBaselineFallRate;
uniform float uMinimumBeatLevel;
uniform float uMinimumBaselineDelta;
uniform float uBaselineThresholdScale;
uniform float uMinimumIntensityDelta;
uniform float uCooldownFrames;
uniform float uBandGainDecibels[10];

out vec4 oState;

const float FFT_SIZE = 2048.0;
const int SAMPLE_COUNT = 48;
const int BAND_COUNT = 10;
const vec2 BAND_FREQUENCIES[BAND_COUNT] = vec2[10](
    vec2(20.0, 40.0),
    vec2(40.0, 80.0),
    vec2(80.0, 160.0),
    vec2(160.0, 320.0),
    vec2(320.0, 640.0),
    vec2(640.0, 1250.0),
    vec2(1250.0, 2500.0),
    vec2(2500.0, 5000.0),
    vec2(5000.0, 10000.0),
    vec2(10000.0, 20000.0));

float readBandLevel(int band) {
  float spectrumWidth = float(textureSize(uSpectrum, 0).x);
  float firstBin = BAND_FREQUENCIES[band].x * FFT_SIZE / uSampleRate;
  float lastBin = BAND_FREQUENCIES[band].y * FFT_SIZE / uSampleRate;
  float power = 0.0;

  for (int sampleIndex = 0; sampleIndex < SAMPLE_COUNT; ++sampleIndex) {
    float position = (float(sampleIndex) + 0.5) / float(SAMPLE_COUNT);
    float bin = mix(firstBin, lastBin, position);
    float spectrumU = (bin + 0.5) / spectrumWidth;
    float amplitude = texture(uSpectrum, vec2(spectrumU, 0.5)).r;
    power += amplitude * amplitude;
  }

  float rms = sqrt(power / float(SAMPLE_COUNT));
  float decibels = 20.0 * log(max(rms, 1e-7)) / log(10.0);
  float range = max(uDecibelRange.y - uDecibelRange.x, 1e-4);
  float level = clamp(
      (decibels + uBandGainDecibels[band] - uDecibelRange.x) / range,
      0.0, 1.0);
  return pow(level, max(uIntensityGamma, 1e-4));
}

void main() {
  int band = clamp(int(gl_FragCoord.x), 0, BAND_COUNT - 1);
  vec4 previous = texelFetch(uPreviousState, ivec2(band, 0), 0);
  float level = readBandLevel(band);

  float smoothing = level > previous.x
      ? uIntensityAttackRate : uIntensityReleaseRate;
  float intensity = mix(previous.x, level, smoothing);

  float baselineRate = level > previous.y
      ? uBaselineRiseRate : uBaselineFallRate;
  float baseline = mix(previous.y, level, baselineRate);

  float cooldown = max(previous.w - 1.0, 0.0);
  float threshold = max(uMinimumBaselineDelta,
                        previous.y * uBaselineThresholdScale);
  bool initialized = previous.x > 0.001 || previous.y > 0.001;
  bool isBeat = initialized && cooldown < 0.5
      && level > uMinimumBeatLevel
      && level - previous.y > threshold
      && level - previous.x > uMinimumIntensityDelta;

  if (isBeat)
    cooldown = uCooldownFrames;

  oState = vec4(intensity, baseline, isBeat ? 1.0 : 0.0, cooldown);
}
