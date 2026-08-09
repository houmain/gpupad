#version 450

layout(binding=0) uniform sampler2D uAlbedo;
layout(binding=1) uniform sampler2D uNormal;
layout(binding=2) uniform sampler2D uAmbientOcclusion;
layout(binding=3) uniform sampler2D uMetalRoughness;
layout(binding=4) uniform sampler2D uEmissive;
layout(binding=5) uniform sampler2DShadow uShadowMap;

uniform mat4 uLightView;

in vec2 vTexCoords;
in vec3 vNormal;
in vec3 vWorldPosition;
in vec4 vShadowCoord;
out vec4 oColor;

vec2 poissonDisk[16] = vec2[]( 
   vec2( -0.94201624, -0.39906216 ), 
   vec2( 0.94558609, -0.76890725 ), 
   vec2( -0.094184101, -0.92938870 ), 
   vec2( 0.34495938, 0.29387760 ), 
   vec2( -0.91588581, 0.45771432 ), 
   vec2( -0.81544232, -0.87912464 ), 
   vec2( -0.38277543, 0.27676845 ), 
   vec2( 0.97484398, 0.75648379 ), 
   vec2( 0.44323325, -0.97511554 ), 
   vec2( 0.53742981, -0.47373420 ), 
   vec2( -0.26496911, -0.41893023 ), 
   vec2( 0.79197514, 0.19090188 ), 
   vec2( -0.24188840, 0.99706507 ), 
   vec2( -0.81409955, 0.91437590 ), 
   vec2( 0.19984126, 0.78641367 ), 
   vec2( 0.14383161, -0.14100790 ) 
);

mat3 cotangentFrame(vec3 normal, vec3 position, vec2 texCoords) {
  vec3 positionDx = dFdx(position);
  vec3 positionDy = dFdy(position);
  vec2 texCoordsDx = dFdx(texCoords);
  vec2 texCoordsDy = dFdy(texCoords);
  float determinant = texCoordsDx.x * texCoordsDy.y - texCoordsDy.x * texCoordsDx.y;
  if (abs(determinant) < 1e-8)
    return mat3(vec3(0), vec3(0), normal);
  vec3 tangent = (texCoordsDy.y * positionDx - texCoordsDx.y * positionDy) / determinant;
  tangent = normalize(tangent - normal * dot(normal, tangent));
  vec3 bitangent = normalize(cross(normal, tangent));
  return mat3(tangent, bitangent, normal);
}

void main() {
  vec3 albedo = texture(uAlbedo, vTexCoords).rgb;
  vec3 mappedNormal = texture(uNormal, vTexCoords).xyz * 2.0 - 1.0;
  vec3 normal = normalize(cotangentFrame(normalize(vNormal), vWorldPosition, vTexCoords) * mappedNormal);
  vec3 viewDirection = normalize(vec3(0, 0, 3.5) - vWorldPosition);
  vec3 lightDirection = normalize(vec3(uLightView[0][2], uLightView[1][2], uLightView[2][2]));

  float ambientOcclusion = texture(uAmbientOcclusion, vTexCoords).r;
  vec3 emissive = texture(uEmissive, vTexCoords).rgb;
  vec2 metalRoughness = texture(uMetalRoughness, vTexCoords).bg;
  float diffuse = 0.9 * max(dot(normal, lightDirection), 0.0) * (1.0 - metalRoughness.x);
  vec3 specularColor = mix(vec3(0.04), albedo, metalRoughness.x);
  float cosAlpha = clamp(dot(viewDirection, reflect(-lightDirection, normal)), 0.0, 1.0);
  float specular = pow(cosAlpha, mix(5.0, 2.0, metalRoughness.y * metalRoughness.y));
  
  float bias = 0.0025;
  float visibility = 1.0;
  for (int i = 0; i < 4; i++)
    visibility -= 0.2 * (1.0 - texture(uShadowMap,
      vec3(vShadowCoord.xy + poissonDisk[i] / 700.0,
      (vShadowCoord.z - bias) / vShadowCoord.w)));
  vec3 color = albedo * (0.2 * ambientOcclusion);
  color += visibility * (albedo * diffuse + specularColor * specular);
  color += emissive;

  oColor = vec4(color, 1.0);  
}
