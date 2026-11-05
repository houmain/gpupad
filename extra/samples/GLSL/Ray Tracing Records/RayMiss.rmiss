#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "RayPayload.glsl"

layout(location = 0) rayPayloadInEXT RayPayload Payload;

void main()
{
    const float t = 0.5 * (normalize(gl_WorldRayDirectionEXT).y + 1.0);
    Payload.Color = mix(vec3(0.04, 0.05, 0.08),
        vec3(0.18, 0.22, 0.32), t);
}
