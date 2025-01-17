#version 450
#extension GL_ARB_separate_shader_objects : enable

// Structs
struct AlphaFragment {
    vec4 color;
    float depth;
    uint next;
    float _pad[2];
};

layout(location = 0) out vec4 fragColor;

layout(std430, set = 0, binding = 0) coherent readonly buffer AlphaFragments
{
    uint count;
    vec3 _pad;
    AlphaFragment fragments[];
}
alphaFragments;

layout(set = 0, binding = 1, r32ui) uniform readonly uimage2D alphaHeadPointer;

float alphaDepth(uint idx)
{
    // TODO: inverted depth here
    return -alphaFragments.fragments[idx].depth;
}

vec4 alphaColor(uint idx)
{
    return alphaFragments.fragments[idx].color;
}

void swap(inout uint a, inout uint b)
{
    uint tmp = a;
    a = b;
    b = tmp;
}

void main()
{
    // Retrieve all Alpha Fragments
    uint alphaHeadPtr = imageLoad(alphaHeadPointer, ivec2(gl_FragCoord.xy)).r;
    const uint MAX_FRAGMENT_COUNT = 32;
    uint alphaFragmentIndices[MAX_FRAGMENT_COUNT];
    uint alphaFragmentIndexCount = 0;
    while (alphaHeadPtr > 0 && alphaFragmentIndexCount < MAX_FRAGMENT_COUNT) {
        alphaFragmentIndices[alphaFragmentIndexCount] = alphaHeadPtr;
        alphaHeadPtr = alphaFragments.fragments[alphaHeadPtr].next;
        ++alphaFragmentIndexCount;
    }

    // Sort Alpha Fragments by Depth (biggest depth first)
    if (alphaFragmentIndexCount > 1) {
        for (uint i = 0; i < alphaFragmentIndexCount - 1; i++) {
            for (uint j = 0; j < alphaFragmentIndexCount - i - 1; j++) {
                if (alphaDepth(alphaFragmentIndices[j]) > alphaDepth(alphaFragmentIndices[j + 1])) {
                    swap(alphaFragmentIndices[j], alphaFragmentIndices[j + 1]);
                }
            }
        }
    }

    vec4 blendedColor = vec4(0.0);
    for (uint i = 0; i < alphaFragmentIndexCount; i++) {
        vec4 alphaFragColor = alphaColor(alphaFragmentIndices[i]);
        blendedColor = mix(blendedColor, alphaFragColor, alphaFragColor.a);
    }
    
    fragColor = blendedColor;
}
