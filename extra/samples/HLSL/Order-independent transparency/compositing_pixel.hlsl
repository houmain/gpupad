
ByteAddressBuffer alphaFragments : register(t0);
RWTexture2D<uint> alphaHeadPointer : register(u0);

float fragmentDepth(uint index)
{
    return -asfloat(alphaFragments.Load(16 + index * 32 + 16));
}

float4 fragmentColor(uint index)
{
    return asfloat(alphaFragments.Load4(16 + index * 32));
}

uint nextFragment(uint index)
{
    return alphaFragments.Load(16 + index * 32 + 20);
}

float4 main(float4 position : SV_Position) : SV_Target0
{
    static const uint maxFragmentCount = 32;
    uint indices[maxFragmentCount];
    uint count = 0;
    uint head = alphaHeadPointer[uint2(position.xy)];
    while (head > 0 && count < maxFragmentCount) {
        indices[count++] = head;
        head = nextFragment(head);
    }

    for (uint i = 0; i + 1 < count; ++i) {
        for (uint j = 0; j + 1 < count - i; ++j) {
            if (fragmentDepth(indices[j]) > fragmentDepth(indices[j + 1])) {
                uint temporary = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = temporary;
            }
        }
    }

    float4 blendedColor = 0.0;
    for (uint i = 0; i < count; ++i) {
        float4 color = fragmentColor(indices[i]);
        blendedColor = lerp(blendedColor, color, color.a);
    }
    return blendedColor;
}
