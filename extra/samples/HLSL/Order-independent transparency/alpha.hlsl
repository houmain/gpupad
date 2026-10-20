
RWByteAddressBuffer alphaFragments : register(u0);
RWTexture2D<uint> alphaHeadPointer : register(u1);

struct FragmentInput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float3 worldNormal : NORMAL0;
    float3 worldPosition : POSITION0;
    float3 worldView : TEXCOORD0;
};

float4 shadeFragment(FragmentInput input)
{
    const float3 lightPosition = float3(-30.0, 30.0, 30.0);
    const float shininess = 5.0;
    float3 normal = normalize(input.worldNormal);
    float3 lightDirection = normalize(-lightPosition);
    float diffuse = dot(lightDirection, normal);
    float3 reflectionDirection = reflect(lightDirection, normal);
    float specular = (shininess + 2.0) * 0.5
        * pow(max(dot(reflectionDirection, input.worldView), 0.0), shininess);

    if (diffuse < 0.0) {
        diffuse *= -0.5;
        specular *= 0.5;
    }

    float3 ambient = input.color.rgb * 0.1;
    float3 lit = ambient + diffuse * input.color.rgb + specular * 0.2;
    return float4(lit, input.color.a);
}

void main(FragmentInput input)
{
    uint nodeIndex;
    alphaFragments.InterlockedAdd(0, 1, nodeIndex);
    ++nodeIndex;

    uint bufferSize;
    alphaFragments.GetDimensions(bufferSize);
    uint nodeOffset = 16 + nodeIndex * 32;
    if (nodeOffset + 24 > bufferSize)
        return;

    float4 color = shadeFragment(input);
    alphaFragments.Store4(nodeOffset, asuint(color));
    alphaFragments.Store(nodeOffset + 16, asuint(input.position.z));

    uint previousHead;
    InterlockedExchange(alphaHeadPointer[uint2(input.position.xy)], nodeIndex, previousHead);
    alphaFragments.Store(nodeOffset + 20, previousHead);
}
