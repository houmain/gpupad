#version 400

subroutine vec4 FadeType(vec2 tc);
subroutine uniform FadeType fadeType;

in vec2 vTexCoords;
out vec4 oColor;


subroutine(FadeType) vec4 fade1(vec2 tc) {
  return vec4(0, 1 - tc.t, 0.7, 1);
}

subroutine(FadeType) vec4 fade2(vec2 tc) {
  return vec4(1, 1 - tc.s, 0, 1);
}

void main() {
  oColor = fadeType(vTexCoords);
}
