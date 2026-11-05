#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "RayPayload.glsl"

layout(shaderRecordEXT, std430) buffer HitRecordData
{
    vec4 HitColor;
};

hitAttributeEXT vec2 HitAttributes;
layout(location = 0) rayPayloadInEXT RayPayload Payload;

void main()
{
    const float edgeShade = 0.65 + 0.35 *
        (1.0 - HitAttributes.x - HitAttributes.y);
    Payload.Color = HitColor.rgb * edgeShade;
}
