
RWTexture2D<unorm float4> uImageR : register(u0);
RWTexture2D<unorm float4> uImageW : register(u1);

[numthreads(16, 16, 1)]
void main(uint2 position : SV_DispatchThreadID)
{
    uint width;
    uint height;
    uImageR.GetDimensions(width, height);
    int2 size = int2(width, height);
    int2 pos = int2(position);

    bool alive = false;
    int neighbors = 0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            int2 wrapped = (pos + int2(x, y) + size) % size;
            if (uImageR[wrapped].r == 0.0) {
                if (x == 0 && y == 0)
                    alive = true;
                else
                    ++neighbors;
            }
        }
    }

    alive = alive ? neighbors >= 2 && neighbors <= 3 : neighbors == 3;
    if (floor(length(float2(pos - size / 2))) == size.x / 3)
        alive = !alive;

    uImageW[pos] = float4((alive ? 0.0 : 1.0).xxx, 1.0);
}
