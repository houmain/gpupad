StructuredBuffer<uint> Values[] : register(t0, space1);
StructuredBuffer<uint> Selection : register(t0);
RWStructuredBuffer<uint> Results : register(u0);

uint lookup(uint index)
{
    return Values[NonUniformResourceIndex(Selection[index])][0];
}
