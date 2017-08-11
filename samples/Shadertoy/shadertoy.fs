#version 330

uniform sampler2D target;
uniform vec4 iMouse;
uniform float iGlobalTime;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform sampler2D iChannel3;
out vec4 oColor;

vec2 iResolution;
int iFrame;

void mainImage(out vec4 fragColor, in vec2 fragCoord);

void main() {
  iResolution = textureSize(target, 0);
  iFrame = int(iGlobalTime * 60.0);
  mainImage(oColor, vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y));
  oColor.a = 1;
}

#define smooth smooth_