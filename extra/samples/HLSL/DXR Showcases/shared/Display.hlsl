float3 displayColor(float3 light)
{
    return sqrt(light / (1 + light));
}
