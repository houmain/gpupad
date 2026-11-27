float studioLobe(float3 direction, float3 axis, float power)
{
    return pow(saturate(dot(direction, normalize(axis))), power);
}

float studioPanel(float3 direction, float offset, float inverseWidth, float low, float high)
{
    return saturate(1 - inverseWidth * abs(direction.x + offset))
        * smoothstep(low, high, direction.y);
}
