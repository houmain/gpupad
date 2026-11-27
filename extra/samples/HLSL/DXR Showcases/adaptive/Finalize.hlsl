StructuredBuffer<uint> ActiveCount : register(t0);
RWStructuredBuffer<uint3> DispatchDimensions : register(u0);

[numthreads(1, 1, 1)]
void main()
{
    // dispatch dimensions for dxr executeindirect (d3d12_dispatch_rays_desc)
    // the host copies this uint3 into width, height and depth
    DispatchDimensions[0] = uint3(ActiveCount[0], 1, 1);
}
