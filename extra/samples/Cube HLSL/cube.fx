
matrix uModel;
matrix uView;
matrix uProjection;
float3 uAmbient;

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
  output.position = mul(mul(mul(uProjection, uView), uModel), input.position);
  output.normal = mul(uModel, float4(input.normal, 0)).xyz;
  output.texCoords = input.texCoords;
  return output;
}

float4 PS(PS_INPUT input) : SV_Target {
  const float3 light = float3(-0.71, 0.71, 0);
  float3 color = uAmbient + uTexture.Sample(sampleLinear, input.texCoords).rgb;
  color *= 0.1 + 0.9 * max(dot(input.normal, light), 0.0);
  return float4(color, 1);
}
