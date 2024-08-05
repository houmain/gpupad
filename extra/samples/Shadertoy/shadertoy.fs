#version 330

uniform vec3  iResolution;               // viewport resolution (in pixels)
/*uniform*/ float iTime;                 // shader playback time (in seconds)
/*uniform*/ float iTimeDelta;            // render time (in seconds)
uniform int       iFrame;                // shader playback frame
/*uniform*/ float iChannelTime[4];       // channel playback time (in seconds)
/*uniform*/ vec3  iChannelResolution[4]; // channel resolution (in pixels)
/*uniform*/ vec4  iMouse = vec4(0);      // mouse pixel coords. xy: current (if MLB down), zw: click
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform sampler2D iChannel3;             // input channel. XX = 2D/Cube
uniform vec4      iDate;                 // (year, month, day, time in seconds)
/*uniform*/ float iSampleRate;           // sound sample rate (i.e., 44100)

out vec4 oColor;

void mainImage(out vec4 fragColor, in vec2 fragCoord);

void main() {
  iTime = iFrame / 60.0;
  iTimeDelta = 1 / 60.0;
  iChannelResolution[0] = vec3(textureSize(iChannel0, 0), 1);
  iChannelResolution[1] = vec3(textureSize(iChannel1, 0), 1);
  iChannelResolution[2] = vec3(textureSize(iChannel2, 0), 1);
  iChannelResolution[3] = vec3(textureSize(iChannel3, 0), 1);
  mainImage(oColor, vec2(gl_FragCoord.x, gl_FragCoord.y));
  oColor.a = 1;
}

#define smooth smooth_