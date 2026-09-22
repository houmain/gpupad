
cbuffer Globals : register(b0) {
    row_major float4x4 Projection;
    row_major float4x4 Modelview;
    row_major float3x3 NormalMatrix;
    float3 LightPosition;
    float TessLevelInner;
    float TessLevelOuter;
    float3 DiffuseMaterial;
    float3 AmbientMaterial;
};

struct VertexOutput {
    float3 position : POSITION0;
};

VertexOutput VS(float3 position : TEXCOORD0) {
    VertexOutput output;
    output.position = position;
    return output;
}

struct HullConstants {
    float edge[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

HullConstants PatchConstants(InputPatch<VertexOutput, 3> patch) {
    HullConstants output;
    output.edge[0] = TessLevelOuter;
    output.edge[1] = TessLevelOuter;
    output.edge[2] = TessLevelOuter;
    output.inside = TessLevelInner;
    return output;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("PatchConstants")]
VertexOutput HS(InputPatch<VertexOutput, 3> patch, uint id : SV_OutputControlPointID) {
    return patch[id];
}

struct DomainOutput {
    float4 clipPosition : SV_Position;
    float3 position : POSITION0;
    float3 patchDistance : TEXCOORD0;
};

[domain("tri")]
DomainOutput DS(HullConstants constants, const OutputPatch<VertexOutput, 3> patch,
                float3 barycentric : SV_DomainLocation) {
    DomainOutput output;
    output.patchDistance = barycentric;
    output.position = normalize(
        barycentric.x * patch[0].position +
        barycentric.y * patch[1].position +
        barycentric.z * patch[2].position);
    output.clipPosition = mul(float4(output.position, 1.0), mul(Modelview, Projection));
    return output;
}

struct GeometryOutput {
    float4 clipPosition : SV_Position;
    float3 patchDistance : TEXCOORD0;
    float3 triangleDistance : TEXCOORD1;
    float3 facetNormal : TEXCOORD2;
};

[maxvertexcount(3)]
void GS(triangle DomainOutput input[3], inout TriangleStream<GeometryOutput> stream) {
    float3 a = input[2].position - input[0].position;
    float3 b = input[1].position - input[0].position;
    float3 normal = mul(normalize(cross(a, b)), NormalMatrix);

    [unroll]
    for (uint i = 0; i < 3; ++i) {
        GeometryOutput output;
        output.clipPosition = input[i].clipPosition;
        output.patchDistance = input[i].patchDistance;
        output.triangleDistance = float3(i == 0, i == 1, i == 2);
        output.facetNormal = normal;
        stream.Append(output);
    }
}

float amplify(float distanceValue, float scale, float offset) {
    float d = saturate(scale * distanceValue + offset);
    return 1.0 - exp2(-2.0 * d * d);
}

float4 PS(GeometryOutput input) : SV_Target0 {
    float3 normal = normalize(input.facetNormal);
    float diffuse = abs(dot(normal, LightPosition));
    float3 color = AmbientMaterial + diffuse * DiffuseMaterial;
    float triangleEdge = min(input.triangleDistance.x,
                             min(input.triangleDistance.y, input.triangleDistance.z));
    float patchEdge = min(input.patchDistance.x,
                          min(input.patchDistance.y, input.patchDistance.z));
    color *= amplify(triangleEdge, 40.0, -0.5) * amplify(patchEdge, 60.0, -0.5);
    return float4(color, 1.0);
}
