#include "lookup.hlsl"

[numthreads(3, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    Results[id.x] = lookup(id.x);
}
