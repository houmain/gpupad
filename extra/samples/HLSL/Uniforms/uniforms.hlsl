
#define assertCmp(CMP, X, Y) if (CMP); else printf("Assertion %.1f == %.1f failed in line %d.", (X), (Y), __LINE__)
#define assertEq(X, Y) assertCmp((X) == (Y), X, Y)
#define assertEq2(X, Y) assertCmp(all(bool2((X) == (Y))), X, Y)
#define assertEq3(X, Y) assertCmp(all(bool3((X) == (Y))), X, Y)

struct Type2 {
  float3 z;
  float2 w;
};

struct Type {
  float x[2];
  Type2 y[2];
};

uniform float u_values_0[2][2];

uniform Type u_values_1[2];
uniform float2 u_values_2;
uniform float3 u_values_3[2];
uniform float3x3 u_values_4;

struct u_block_t {
  float values_0[2][2];
  Type values_1[2];
  float2 values_2;
};

struct u_block_2_t {
  float values_0[2];
};

ConstantBuffer<u_block_t> u_block;
ConstantBuffer<u_block_2_t> u_block_2[2];

[numthreads(1, 1, 1)]
void main() {
  assertEq(u_values_0[0][0], 1.2);
  assertEq(u_values_0[0][1], 2.0);
  assertEq(u_values_0[1][0], 3.1);
  assertEq(u_values_0[1][1], 4.2);

  assertEq(u_values_1[0].x[0], float(1));
  assertEq(u_values_1[0].x[1], float(2));
  assertEq3(u_values_1[0].y[0].z, float3(3, 4, 5));
  assertEq2(u_values_1[0].y[0].w, float2(6, 7));
  assertEq3(u_values_1[0].y[1].z, float3(8, 9, 10));
  assertEq2(u_values_1[0].y[1].w, float2(11, 12));  
  assertEq(u_values_1[1].x[0], float(13));
  assertEq(u_values_1[1].x[1], float(14));
  assertEq3(u_values_1[1].y[0].z, float3(15, 16, 17));
  assertEq2(u_values_1[1].y[0].w, float2(18, 19));
  assertEq3(u_values_1[1].y[1].z, float3(20, 21, 22));
  assertEq2(u_values_1[1].y[1].w, float2(23, 24));

  assertEq2(u_values_2, float2(1.0, 2.0));
  
  assertEq3(u_values_3[0], float3(1.0, 2.0, 3.0));
  assertEq3(u_values_3[1], float3(4.1, 5.1, 6.1));

  assertEq3(u_values_4[0], float3(1, 4, 7));
  assertEq3(u_values_4[1], float3(2, 5, 8));
  assertEq3(u_values_4[2], float3(3, 6, 9));  

  assertEq(u_block.values_0[0][0], 1.2);
  assertEq(u_block.values_0[0][1], 2.0);
  assertEq(u_block.values_0[1][0], 3.1);
  assertEq(u_block.values_0[1][1], 4.2);
  
  assertEq(u_block.values_1[0].x[0], float(1));
  assertEq(u_block.values_1[0].x[1], float(2));
  assertEq3(u_block.values_1[0].y[0].z, float3(3, 4, 5));
  assertEq2(u_block.values_1[0].y[0].w, float2(6, 7));
  assertEq3(u_block.values_1[0].y[1].z, float3(8, 9, 10));
  assertEq2(u_block.values_1[0].y[1].w, float2(11, 12));  
  assertEq(u_block.values_1[1].x[0], float(13));
  assertEq(u_block.values_1[1].x[1], float(14));
  assertEq3(u_block.values_1[1].y[0].z, float3(15, 16, 17));
  assertEq2(u_block.values_1[1].y[0].w, float2(18, 19));
  assertEq3(u_block.values_1[1].y[1].z, float3(20, 21, 22));
  assertEq2(u_block.values_1[1].y[1].w, float2(23, 24));

  assertEq2(u_block.values_2, float2(1.0, 2.0));

  assertEq(u_block_2[0].values_0[0], float(1));
  assertEq(u_block_2[0].values_0[1], float(2));
  assertEq(u_block_2[1].values_0[0], float(3));
  assertEq(u_block_2[1].values_0[1], float(4.1));
 
  printf("All tests completed.");
}
