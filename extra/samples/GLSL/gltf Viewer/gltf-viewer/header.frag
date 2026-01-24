#version 330

#define varying in
#define texture2D texture
#define textureCube texture
#define textureCubeLodEXT textureLod
#define USE_TEX_LOD

#define gl_FragColor out_color

out vec4 out_color;

#define MATERIAL_METALLICROUGHNESS
#define HAS_BASE_COLOR_MAP
#define HAS_NORMAL_MAP
#define HAS_METALLIC_ROUGHNESS_MAP
#define HAS_EMISSIVE_MAP
#define HAS_OCCLUSION_MAP

#define USE_IBL
#define USE_HDR
#define USE_PUNCTUAL
#define LIGHT_COUNT 1

#define TONEMAP_UNCHARTED

#define HAS_NORMALS
//#define HAS_TANGENTS

//#define MATERIAL_UNLIT

//#define DEBUG_OUTPUT
//#define DEBUG_METALLIC
//#define DEBUG_ROUGHNESS
//#define DEBUG_NORMAL
//#define DEBUG_BASECOLOR
//#define DEBUG_OCCLUSION
//#define DEBUG_EMISSIVE
//#define DEBUG_F0