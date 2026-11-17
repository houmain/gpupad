cbuffer Settings : register(b0)
{
    uint width;
};

RWStructuredBuffer<uint> Dimensions : register(u0);

[numthreads(1, 1, 1)]
void main()
{
    Dimensions[6] = width;
    Dimensions[7] = 1;
    Dimensions[8] = 1;
}
