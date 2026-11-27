#ifndef STUDIO_PAYLOAD_HLSL
#define STUDIO_PAYLOAD_HLSL

static const uint floorMaterial = 3;
static const uint invalidInstance = 0xffffffffu;
// positive tmin (rtgbook, #1, section 6.2.2.2)
// fixed shading-normal offset (rtgbook, #1, section 6.2.2.3)
static const float rayBias = 0.002;
static const float sceneRange = 60.0;

struct RayPayload
{
    float3 Normal;
    float Distance;
    uint Instance;
};

#endif
