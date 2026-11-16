#include "lookup.hlsl"

[shader("raygeneration")]
void main()
{
    uint index = DispatchRaysIndex().x;
    Results[3 + index] = lookup(index);
}
