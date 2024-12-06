
#include "Syntax.h"

namespace {

    QStringList keywords()
    {
        return { "const", "uniform", "buffer", "shared", "attribute", "varying",
            "coherent", "volatile", "restrict", "readonly", "writeonly",
            "atomic_uint", "layout", "centroid", "flat", "smooth",
            "noperspective", "patch", "sample", "invariant", "precise", "break",
            "continue", "do", "for", "while", "switch", "case", "default", "if",
            "else", "subroutine", "in", "out", "inout", "int", "void", "bool",
            "true", "false", "float", "double", "discard", "return", "vec2",
            "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3",
            "bvec4", "uint", "uvec2", "uvec3", "uvec4", "dvec2", "dvec3",
            "dvec4", "mat2", "mat3", "mat4", "mat2x2", "mat2x3", "mat2x4",
            "mat3x2", "mat3x3", "mat3x4", "mat4x2", "mat4x3", "mat4x4", "dmat2",
            "dmat3", "dmat4", "dmat2x2", "dmat2x3", "dmat2x4", "dmat3x2",
            "dmat3x3", "dmat3x4", "dmat4x2", "dmat4x3", "dmat4x4", "lowp",
            "mediump", "highp", "precision", "sampler1D", "sampler1DShadow",
            "sampler1DArray", "sampler1DArrayShadow", "isampler1D",
            "isampler1DArray", "usampler1D", "usampler1DArray", "sampler2D",
            "sampler2DShadow", "sampler2DArray", "sampler2DArrayShadow",
            "isampler2D", "isampler2DArray", "usampler2D", "usampler2DArray",
            "sampler2DRect", "sampler2DRectShadow", "isampler2DRect",
            "usampler2DRect", "sampler2DMS", "isampler2DMS", "usampler2DMS",
            "sampler2DMSArray", "isampler2DMSArray", "usampler2DMSArray",
            "sampler3D", "isampler3D", "usampler3D", "samplerCube",
            "samplerCubeShadow", "isamplerCube", "usamplerCube",
            "samplerCubeArray", "samplerCubeArrayShadow", "isamplerCubeArray",
            "usamplerCubeArray", "samplerBuffer", "isamplerBuffer",
            "usamplerBuffer", "image1D", "iimage1D", "uimage1D", "image1DArray",
            "iimage1DArray", "uimage1DArray", "image2D", "iimage2D", "uimage2D",
            "image2DArray", "iimage2DArray", "uimage2DArray", "image2DRect",
            "iimage2DRect", "uimage2DRect", "image2DMS", "iimage2DMS",
            "uimage2DMS", "image2DMSArray", "iimage2DMSArray",
            "uimage2DMSArray", "image3D", "iimage3D", "uimage3D", "imageCube",
            "iimageCube", "uimageCube", "imageCubeArray", "iimageCubeArray",
            "uimageCubeArray", "imageBuffer", "iimageBuffer", "uimageBuffer",
            "struct", 
        
            // when targeting Vulkan
            "texture1D", "texture1DArray", "itexture1D", "itexture1DArray",
            "utexture1D", "utexture1DArray", "texture2D", "texture2DArray",
            "itexture2D", "itexture2DArray", "utexture2D", "utexture2DArray",
            "texture2DRect", "itexture2DRect", "utexture2DRect", "texture2DMS",
            "itexture2DMS", "utexture2DMS", "texture2DMSArray",
            "itexture2DMSArray", "utexture2DMSArray", "texture3D", "itexture3D",
            "utexture3D", "textureCube", "itextureCube", "utextureCube",
            "textureCubeArray", "itextureCubeArray", "utextureCubeArray",
            "textureBuffer", "itextureBuffer", "utextureBuffer", "sampler",
            "samplerShadow", "subpassInput", "isubpassInput", "usubpassInput",
            "subpassInputMS", "isubpassInputMS", "usubpassInputMS",

            // reserved for future use
            "common", "partition", "active", "asm", "class", "union", "enum",
            "typedef", "template", "this", "resource", "goto", "inline",
            "noinline", "public", "static", "extern", "external", "interface",
            "long", "short", "half", "fixed", "unsigned", "superp", "input",
            "output", "hvec2", "hvec3", "hvec4", "fvec2", "fvec3", "fvec4",
            "filter", "sizeof", "cast", "namespace", "using", "sampler3DRect" };
    }

    QStringList builtinFunctions()
    {
        return { "printf", "abs", "acos", "acosh", "all", "any", "asin",
            "asinh", "atan", "atanh", "atomicCounter", "atomicCounterDecrement",
            "atomicAdd", "atomicAnd", "atomicOr", "atomicXor", "atomicMin",
            "atomicMax", "atomicExchange", "atomicCompSwap",
            "atomicCounterIncrement", "barrier", "bitCount", "bitfieldExtract",
            "bitfieldInsert", "bitfieldReverse", "ceil", "clamp", "cos", "cosh",
            "cross", "dFdx", "dFdy", "degrees", "determinant", "distance",
            "dot", "equal", "exp", "exp2", "faceforward", "findLSB", "findMSB",
            "floatBitsToInt", "floatBitsToUint", "floor", "fma", "fract",
            "frexp", "ftransform", "fwidth", "greaterThan", "greaterThanEqual",
            "imageAtomicAdd", "imageAtomicAnd", "imageAtomicCompSwap",
            "imageAtomicExchange", "imageAtomicMax", "imageAtomicMin",
            "imageAtomicOr", "imageAtomicXor", "imageSize", "imageLoad",
            "imageStore", "imulExtended", "intBitsToFloat",
            "interpolateAtCentroid", "interpolateAtOffset",
            "interpolateAtSample", "inverse", "inversesqrt", "isinf", "isnan",
            "ldexp", "length", "lessThan", "lessThanEqual", "log", "log2",
            "matrixCompMult", "max", "memoryBarrier", "min", "mix", "mod",
            "modf", "noise1", "noise2", "noise3", "noise4", "normalize", "not",
            "notEqual", "outerProduct", "packDouble2x32", "packHalf2x16",
            "packSnorm2x16", "packSnorm4x8", "packUnorm2x16", "packUnorm4x8",
            "pow", "radians", "reflect", "refract", "round", "roundEven",
            "shadow1D", "shadow1DLod", "shadow1DProj", "shadow1DProjLod",
            "shadow2D", "shadow2DLod", "shadow2DProj", "shadow2DProjLod",
            "sign", "sin", "sinh", "smoothstep", "sqrt", "step", "tan", "tanh",
            "texelFetch", "texelFetchOffset", "texture", "texture1D",
            "texture1DLod", "texture1DProj", "texture1DProjLod", "texture2D",
            "texture2DLod", "texture2DProj", "texture2DProjLod", "texture3D",
            "texture3DLod", "texture3DProj", "texture3DProjLod", "textureCube",
            "textureCubeLod", "textureGather", "textureGatherOffset",
            "textureGatherOffsets", "textureGrad", "textureGradOffset",
            "textureLod", "textureLodOffset", "textureOffset", "textureProj",
            "textureProjGrad", "textureProjGradOffset", "textureProjLod",
            "textureProjLodOffset", "textureProjOffset", "textureQueryLod",
            "textureSize", "transpose", "trunc", "uaddCarry", "uintBitsToFloat",
            "umulExtended", "unpackDouble2x32", "unpackHalf2x16",
            "unpackSnorm2x16", "unpackSnorm4x8", "unpackUnorm2x16",
            "unpackUnorm4x8", "usubBorrow" };
    }

    QStringList builtinConstants()
    {
        return { "printfEnabled", "HAS_PRINTF", "gl_ClipDistance",
            "gl_CullDistance", "gl_DepthRange", "gl_DepthRangeParameters",
            "gl_FragCoord", "gl_FragDepth", "gl_FrontFacing",
            "gl_GlobalInvocationID", "gl_HelperInvocation", "gl_in",
            "gl_InstanceID", "gl_InvocationID", "gl_Layer",
            "gl_LocalInvocationID", "gl_LocalInvocationIndex",
            "gl_MaxAtomicCounterBindings", "gl_MaxAtomicCounterBufferSize",
            "gl_MaxClipDistances", "gl_MaxCombinedAtomicCounterBuffers",
            "gl_MaxCombinedAtomicCounters",
            "gl_MaxCombinedClipAndCullDistances", "gl_MaxCombinedImageUniforms",
            "gl_MaxCombinedImageUnitsAndFragmentOutputs",
            "gl_MaxCombinedShaderOutputResources",
            "gl_MaxCombinedTextureImageUnits",
            "gl_MaxComputeAtomicCounterBuffers", "gl_MaxComputeAtomicCounters",
            "gl_MaxComputeImageUniforms", "gl_MaxComputeTextureImageUnits",
            "gl_MaxComputeUniformComponents", "gl_MaxComputeWorkGroupCount",
            "gl_MaxComputeWorkGroupSize", "gl_MaxCullDistances",
            "gl_MaxDrawBuffers", "gl_MaxFragmentAtomicCounterBuffers",
            "gl_MaxFragmentAtomicCounters", "gl_MaxFragmentImageUniforms",
            "gl_MaxFragmentInputComponents", "gl_MaxFragmentUniformComponents",
            "gl_MaxFragmentUniformVectors",
            "gl_MaxGeometryAtomicCounterBuffers",
            "gl_MaxGeometryAtomicCounters", "gl_MaxGeometryImageUniforms",
            "gl_MaxGeometryInputComponents", "gl_MaxGeometryOutputComponents",
            "gl_MaxGeometryOutputVertices", "gl_MaxGeometryTextureImageUnits",
            "gl_MaxGeometryTotalOutputComponents",
            "gl_MaxGeometryUniformComponents",
            "gl_MaxGeometryVaryingComponents", "gl_MaxImageSamples",
            "gl_MaxImageUnits", "gl_MaxPatchVertices",
            "gl_MaxProgramTexelOffset", "gl_MaxSamples",
            "gl_MaxTessControlAtomicCounterBuffers",
            "gl_MaxTessControlAtomicCounters", "gl_MaxTessControlImageUniforms",
            "gl_MaxTessControlInputComponents",
            "gl_MaxTessControlOutputComponents",
            "gl_MaxTessControlTextureImageUnits",
            "gl_MaxTessControlTotalOutputComponents",
            "gl_MaxTessControlUniformComponents",
            "gl_MaxTessEvaluationAtomicCounterBuffers",
            "gl_MaxTessEvaluationAtomicCounters",
            "gl_MaxTessEvaluationImageUniforms",
            "gl_MaxTessEvaluationInputComponents",
            "gl_MaxTessEvaluationOutputComponents",
            "gl_MaxTessEvaluationTextureImageUnits",
            "gl_MaxTessEvaluationUniformComponents", "gl_MaxTessGenLevel",
            "gl_MaxTessPatchComponents", "gl_MaxTextureImageUnits",
            "gl_MaxTransformFeedbackBuffers",
            "gl_MaxTransformFeedbackInterleavedComponents",
            "gl_MaxVaryingComponents", "gl_MaxVaryingVectors",
            "gl_MaxVertexAtomicCounterBuffers", "gl_MaxVertexAtomicCounters",
            "gl_MaxVertexAttribs", "gl_MaxVertexImageUniforms",
            "gl_MaxVertexOutputComponents", "gl_MaxVertexTextureImageUnits",
            "gl_MaxVertexUniformComponents", "gl_MaxVertexUniformVectors",
            "gl_MaxViewports", "gl_MinProgramTexelOffset", "gl_NumSamples",
            "gl_NumWorkGroups", "gl_out", "gl_PatchVerticesIn", "gl_PerVertex",
            "gl_PointCoord", "gl_PointSize", "gl_Position", "gl_PrimitiveID",
            "gl_PrimitiveIDIn", "gl_SampleID", "gl_SampleMask",
            "gl_SampleMaskIn", "gl_SamplePosition", "gl_TessCoord",
            "gl_TessLevelInner", "gl_TessLevelOuter", "gl_VertexID",
            "gl_ViewportIndex", "gl_WorkGroupID", "gl_WorkGroupSize" };
    }

    QStringList layoutQualifiers()
    {
        return { "row_major", "column_major", "location", "component",
            "binding", "offset", "index", "shared", "packed", "std140",
            "std430", "origin_upper_left", "pixel_center_integer",
            "early_fragment_tests", "xfb_buffer", "xfb_offset", "xfb_stride",
            "vertices", "max_vertices", "points", "lines", "lines_adjacency",
            "triangles", "triangles_adjacency", "line_strip", "triangle_strip",
            "rgba32f", "rgba16f", "rg32f", "rg16f", "r11f_g11f_b10f", "r32f",
            "r16f", "rgba16", "rgb10_a2", "rgba8", "rg16", "rg8", "r16", "r8",
            "rgba16_snorm", "rgba8_snorm", "rg16_snorm", "rg8_snorm",
            "r16_snorm", "r8_snorm", "rgba32i", "rgba16i", "rgba8i", "rg32i",
            "rg16i", "rg8i", "r32i", "r16i", "r8i", "rgba32ui", "rgba16ui",
            "rgb10_a2ui", "rgba8ui", "rg32ui", "rg16ui", "rg8ui", "r32ui",
            "r16ui", "r8ui" };
    }

} // namespace

class SyntaxGLSL : public Syntax
{
public:
    QStringList keywords() const override { return ::keywords(); }
    QStringList builtinFunctions() const override
    {
        return ::builtinFunctions();
    }
    QStringList builtinConstants() const override
    {
        return ::builtinConstants();
    }
    QStringList completerStrings() const override
    {
        return ::keywords() + ::builtinFunctions() + ::builtinConstants()
            + ::layoutQualifiers();
    }
    bool hasPreprocessor() const override { return true; }
};

Syntax *makeSyntaxGLSL()
{
    return new SyntaxGLSL();
}
