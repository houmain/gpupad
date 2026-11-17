RWStructuredBuffer<uint> RayResults : register(u0);

[shader("raygeneration")]
void main()
{
    uint i = DispatchRaysIndex().x;
    RayResults[i] = 100 + i;
}
