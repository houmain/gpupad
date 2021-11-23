
matrix uModel;
matrix uView;
matrix uProjection;
Texture2D uTexture;
SamplerState sampleLinear;

struct VS_INPUT {
  float4 position : POSITION;
  float3 normal : NORMAL;
  float2 texCoords : TEXCOORD0;
};

struct PS_INPUT {
  float4 position : SV_POSITION;
  float3 normal : NORMAL;
  float2 texCoords : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input) {
  PS_INPUT output = (PS_INPUT)0;
  output.position = mul(input.position, uModel);
  output.position = mul(output.position, uView);
  output.position = mul(output.position, uProjection);
  output.normal = (uModel * float4(input.normal, 0)).xyz;
  output.texCoords = input.texCoords;
  return output;
}

float4 PS(PS_INPUT input) : SV_Target {
  float3 color = uTexture.Sample(sampleLinear, input.texCoords).rgb;
  color *= 0.3 + 0.7 * max(dot(input.normal, float3(0, 0, 1)), 0.0);
  return float4(color, 1);
}
