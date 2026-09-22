
struct VertexOutput {
    float4 position : SV_Position;
    float4 color : TEXCOORD0;
};

VertexOutput main(float2 position : TEXCOORD0,
                  float2 instancePosition : TEXCOORD1,
                  float4 instanceColor : TEXCOORD2)
{
    VertexOutput output;
    output.position = float4(position / 3.0 + instancePosition / 2.0, 0.0, 1.0);
    output.color = instanceColor;
    return output;
}
