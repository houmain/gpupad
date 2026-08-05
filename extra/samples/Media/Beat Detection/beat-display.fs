#version 330

uniform sampler2D uState;
uniform sampler2D uHistory;

out vec4 oColor;

const int BAND_COUNT = 10;
const int BAND_HEIGHT = 10;

void main() {
  ivec2 pixel = ivec2(gl_FragCoord.xy);
  ivec2 historySize = textureSize(uHistory, 0);

  // Copy from the pixel to the right so the accumulated history travels left.
  if (pixel.x < historySize.x - 1) {
    oColor = texelFetch(uHistory, pixel + ivec2(1, 0), 0);
    return;
  }

  int band = clamp(pixel.y / BAND_HEIGHT, 0, BAND_COUNT - 1);
  vec4 state = texelFetch(uState, ivec2(band, 0), 0);

  oColor = state.z >= 0.5
      ? vec4(1.0, 0.0, 0.0, 1.0)
      : vec4(vec3(state.x), 1.0);
}
