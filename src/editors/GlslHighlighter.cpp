#include "GlslHighlighter.h"
#include <QCompleter>
#include <QStringListModel>

namespace {
const auto keywords = {
    "attribute", "const", "uniform", "varying", "coherent",
    "volatile", "restrict", "buffer", "readonly", "shared", "writeonly",
    "atomic_uint", "layout", "centroid", "patch", "flat", "smooth",
    "noperspective", "sample", "break", "continue", "do", "for", "while", "if",
    "switch", "case", "default", "else", "subroutine", "in", "out", "inout",
    "float", "double", "invariant", "int", "void", "bool", "true", "false",
    "precise", "discard", "return", "mat2", "mat3", "mat4", "dmat2", "dmat3",
    "dmat4", "mat2x2", "mat2x3", "mat2x4", "dmat2x2", "dmat2x3", "dmat2x4",
    "mat3x2", "mat3x3", "mat3x4", "dmat3x2", "dmat3x3", "dmat3x4", "mat4x2",
    "mat4x3", "mat4x4", "dmat4x2", "dmat4x3", "dmat4x4", "vec2", "vec3", "vec4",
    "uint", "uvec2", "ivec2", "ivec3", "ivec4", "uvec3", "uvec4", "bvec2",
    "bvec3", "bvec4", "dvec2", "dvec3", "dvec4", "lowp", "mediump", "highp",
    "precision", "sampler1D", "sampler2D", "sampler3D", "samplerCube",
    "sampler1DShadow", "sampler2DShadow", "samplerCubeShadow", "sampler1DArray",
    "sampler2DArray", "sampler1DArrayShadow", "sampler2DArrayShadow",
    "isampler1D", "isampler2D", "isampler3D", "isamplerCube", "isampler1DArray",
    "isampler2DArray", "usampler1D", "usampler2D", "usampler3D", "usamplerCube",
    "usampler1DArray", "usampler2DArray", "sampler2DRect", "sampler2DRectShadow",
    "isampler2DRect", "samplerBuffer", "isamplerBuffer", "usamplerBuffer",
    "sampler2DMS", "isampler2DMS", "usampler2DMS", "sampler2DMSArray",
    "isampler2DMSArray", "usampler2DRect", "usampler2DMSArray",
    "samplerCubeArray", "samplerCubeArrayShadow", "isamplerCubeArray",
    "usamplerCubeArray", "image1D", "iimage1D", "uimage1D", "image2D", "iimage2D",
    "uimage2D", "image3D", "iimage3D", "uimage3D", "image2DRect", "imageCube",
    "imageBuffer", "iimage2DRect", "iimageCube", "uimage2DRect", "uimageCube",
    "iimageBuffer", "uimageBuffer", "image1DArray", "iimage1DArray",
    "uimage1DArray", "image2DArray", "iimage2DArray", "uimage2DArray",
    "imageCubeArray", "image2DMS", "iimageCubeArray", "iimage2DMS",
    "image2DMSArray", "uimageCubeArray", "uimage2DMS", "iimage2DMSArray",
    "uimage2DMSArray", "struct" };

const auto builtinFunctions = {
    "abs", "acos", "acosh", "all", "any", "asin",
    "asinh", "atan", "atanh", "atomicCounter", "atomicCounterDecrement",
    "atomicCounterIncrement", "barrier", "bitCount", "bitfieldExtract",
    "bitfieldInsert", "bitfieldReverse", "ceil", "clamp", "cos", "cosh", "cross",
    "dFdx", "dFdy", "degrees", "determinant", "distance", "dot", "equal", "exp",
    "exp2", "faceforward", "findLSB", "findMSB", "floatBitsToInt",
    "floatBitsToUint", "floor", "fma", "fract", "frexp", "ftransform", "fwidth",
    "greaterThan", "greaterThanEqual", "imageAtomicAdd", "imageAtomicAnd",
    "imageAtomicCompSwap", "imageAtomicExchange", "imageAtomicMax",
    "imageAtomicMin", "imageAtomicOr", "imageAtomicXor", "imageLoad",
    "imageStore", "imulExtended", "intBitsToFloat", "interpolateAtCentroid",
    "interpolateAtOffset", "interpolateAtSample", "inverse", "inversesqrt",
    "isinf", "isnan", "ldexp", "length", "lessThan", "lessThanEqual", "log",
    "log2", "matrixCompMult", "max", "memoryBarrier", "min", "mix", "mod", "modf",
    "noise1", "noise2", "noise3", "noise4", "normalize", "not", "notEqual",
    "outerProduct", "packDouble2x32", "packHalf2x16", "packSnorm2x16",
    "packSnorm4x8", "packUnorm2x16", "packUnorm4x8", "pow", "radians", "reflect",
    "refract", "round", "roundEven", "shadow1D", "shadow1DLod", "shadow1DProj",
    "shadow1DProjLod", "shadow2D", "shadow2DLod", "shadow2DProj",
    "shadow2DProjLod", "sign", "sin", "sinh", "smoothstep", "sqrt", "step", "tan",
    "tanh", "texelFetch", "texelFetchOffset", "texture", "texture1D",
    "texture1DLod", "texture1DProj", "texture1DProjLod", "texture2D",
    "texture2DLod", "texture2DProj", "texture2DProjLod", "texture3D",
    "texture3DLod", "texture3DProj", "texture3DProjLod", "textureCube",
    "textureCubeLod", "textureGather", "textureGatherOffset",
    "textureGatherOffsets", "textureGrad", "textureGradOffset", "textureLod",
    "textureLodOffset", "textureOffset", "textureProj", "textureProjGrad",
    "textureProjGradOffset", "textureProjLod", "textureProjLodOffset",
    "textureProjOffset", "textureQueryLod", "textureSize", "transpose", "trunc",
    "uaddCarry", "uintBitsToFloat", "umulExtended", "unpackDouble2x32",
    "unpackHalf2x16", "unpackSnorm2x16", "unpackSnorm4x8", "unpackUnorm2x16",
    "unpackUnorm4x8", "usubBorrow" };

const auto builtinConstants = {
    "gl_ClipDistance", "gl_CullDistance",
    "gl_DepthRange", "gl_DepthRangeParameters", "gl_FragCoord", "gl_FragDepth",
    "gl_FrontFacing", "gl_GlobalInvocationID", "gl_HelperInvocation", "gl_in",
    "gl_InstanceID", "gl_InvocationID", "gl_Layer", "gl_LocalInvocationID",
    "gl_LocalInvocationIndex", "gl_MaxAtomicCounterBindings",
    "gl_MaxAtomicCounterBufferSize", "gl_MaxClipDistances",
    "gl_MaxCombinedAtomicCounterBuffers", "gl_MaxCombinedAtomicCounters",
    "gl_MaxCombinedClipAndCullDistances", "gl_MaxCombinedImageUniforms",
    "gl_MaxCombinedImageUnitsAndFragmentOutputs",
    "gl_MaxCombinedShaderOutputResources", "gl_MaxCombinedTextureImageUnits",
    "gl_MaxComputeAtomicCounterBuffers", "gl_MaxComputeAtomicCounters",
    "gl_MaxComputeImageUniforms", "gl_MaxComputeTextureImageUnits",
    "gl_MaxComputeUniformComponents", "gl_MaxComputeWorkGroupCount",
    "gl_MaxComputeWorkGroupSize", "gl_MaxCullDistances", "gl_MaxDrawBuffers",
    "gl_MaxFragmentAtomicCounterBuffers", "gl_MaxFragmentAtomicCounters",
    "gl_MaxFragmentImageUniforms", "gl_MaxFragmentInputComponents",
    "gl_MaxFragmentUniformComponents", "gl_MaxFragmentUniformVectors",
    "gl_MaxGeometryAtomicCounterBuffers", "gl_MaxGeometryAtomicCounters",
    "gl_MaxGeometryImageUniforms", "gl_MaxGeometryInputComponents",
    "gl_MaxGeometryOutputComponents", "gl_MaxGeometryOutputVertices",
    "gl_MaxGeometryTextureImageUnits", "gl_MaxGeometryTotalOutputComponents",
    "gl_MaxGeometryUniformComponents", "gl_MaxGeometryVaryingComponents",
    "gl_MaxImageSamples", "gl_MaxImageUnits", "gl_MaxPatchVertices",
    "gl_MaxProgramTexelOffset", "gl_MaxSamples",
    "gl_MaxTessControlAtomicCounterBuffers", "gl_MaxTessControlAtomicCounters",
    "gl_MaxTessControlImageUniforms", "gl_MaxTessControlInputComponents",
    "gl_MaxTessControlOutputComponents", "gl_MaxTessControlTextureImageUnits",
    "gl_MaxTessControlTotalOutputComponents",
    "gl_MaxTessControlUniformComponents",
    "gl_MaxTessEvaluationAtomicCounterBuffers",
    "gl_MaxTessEvaluationAtomicCounters", "gl_MaxTessEvaluationImageUniforms",
    "gl_MaxTessEvaluationInputComponents", "gl_MaxTessEvaluationOutputComponents",
    "gl_MaxTessEvaluationTextureImageUnits",
    "gl_MaxTessEvaluationUniformComponents", "gl_MaxTessGenLevel",
    "gl_MaxTessPatchComponents", "gl_MaxTextureImageUnits",
    "gl_MaxTransformFeedbackBuffers",
    "gl_MaxTransformFeedbackInterleavedComponents", "gl_MaxVaryingComponents",
    "gl_MaxVaryingVectors", "gl_MaxVertexAtomicCounterBuffers",
    "gl_MaxVertexAtomicCounters", "gl_MaxVertexAttribs",
    "gl_MaxVertexImageUniforms", "gl_MaxVertexOutputComponents",
    "gl_MaxVertexTextureImageUnits", "gl_MaxVertexUniformComponents",
    "gl_MaxVertexUniformVectors", "gl_MaxViewports", "gl_MinProgramTexelOffset",
    "gl_NumSamples", "gl_NumWorkGroups", "gl_out", "gl_PatchVerticesIn",
    "gl_PerVertex", "gl_PointCoord", "gl_PointSize", "gl_Position",
    "gl_PrimitiveID", "gl_PrimitiveIDIn", "gl_SampleID", "gl_SampleMask",
    "gl_SampleMaskIn", "gl_SamplePosition", "gl_TessCoord", "gl_TessLevelInner",
    "gl_TessLevelOuter", "gl_VertexID", "gl_ViewportIndex", "gl_WorkGroupID",
    "gl_WorkGroupSize" };

const auto layoutQualifiers = {
    "row_major", "column_major", "location", "component", "binding", "offset",
    "index", "shared​", "packed​", "std140​", "std430​", "origin_upper_left",
    "pixel_center_integer", "early_fragment_tests", "xfb_buffer", "xfb_offset",
    "xfb_stride", "vertices", "max_vertices", "points", "lines",
    "lines_adjacency", "triangles", "triangles_adjacency", "line_strip",
    "triangle_strip", "rgba32f", "rgba16f", "rg32f", "rg16f", "r11f_g11f_b10f",
    "r32f", "r16f", "rgba16", "rgb10_a2", "rgba8", "rg16",  "rg8", "r16", "r8",
    "rgba16_snorm", "rgba8_snorm", "rg16_snorm", "rg8_snorm", "r16_snorm",
    "r8_snorm", "rgba32i", "rgba16i", "rgba8i", "rg32i", "rg16i", "rg8i",
    "r32i", "r16i", "r8i", "rgba32ui", "rgba16ui", "rgb10_a2ui", "rgba8ui",
    "rg32ui", "rg16ui", "rg8ui", "r32ui", "r16ui", "r8ui" };
} // namespace

GlslHighlighter::GlslHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent)
{
    QTextCharFormat keywordFormat;
    QTextCharFormat builtinFunctionFormat;
    QTextCharFormat builtinConstantsFormat;
    QTextCharFormat preprocessorFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat functionFormat;

    functionFormat.setForeground(QColor("#000066"));
    functionFormat.setFontWeight(QFont::Bold);
    keywordFormat.setForeground(QColor("#003C98"));
    builtinFunctionFormat.setForeground(QColor("#000066"));
    builtinConstantsFormat.setForeground(QColor("#981111"));
    numberFormat.setForeground(QColor("#981111"));
    quotationFormat.setForeground(QColor("#981111"));
    preprocessorFormat.setForeground(QColor("#800080"));
    singleLineCommentFormat.setForeground(QColor("#008700"));
    mMultiLineCommentFormat.setForeground(QColor("#008700"));

    auto rule = HighlightingRule();
    rule.pattern = QRegExp("\\b[A-Za-z0-9_]+(?=\\()");
    rule.format = functionFormat;
    mHighlightingRules.append(rule);

    auto completerStrings = QStringList();

    for (const auto& keyword : keywords) {
        rule.pattern = QRegExp(QStringLiteral("\\b%1\\b").arg(keyword));
        rule.format = keywordFormat;
        mHighlightingRules.append(rule);
        completerStrings.append(keyword);
    }

    for (const auto& builtinFunction : builtinFunctions) {
        rule.pattern = QRegExp(QStringLiteral("\\b%1\\b").arg(builtinFunction));
        rule.format = builtinFunctionFormat;
        mHighlightingRules.append(rule);
        completerStrings.append(builtinFunction);
    }

    for (const auto& builtinConstant : builtinConstants) {
        rule.pattern = QRegExp(QStringLiteral("\\b%1\\b").arg(builtinConstant));
        rule.format = builtinConstantsFormat;
        mHighlightingRules.append(rule);
        completerStrings.append(builtinConstant);
    }

    for (const auto& qaulifier : layoutQualifiers)
        completerStrings.append(qaulifier);

    rule.pattern = QRegExp("\\b[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?[uUlLfF]{,2}\\b");
    rule.format = numberFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegExp("\\b0x[0-9,A-F,a-f]+\\b");
    rule.format = numberFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegExp("\".*\"");
    rule.format = quotationFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegExp("^\\s*#.*$");
    rule.format = preprocessorFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegExp("//[^\n]*");
    rule.format = singleLineCommentFormat;
    mHighlightingRules.append(rule);

    mCommentStartExpression = QRegExp("/\\*");
    mCommentEndExpression = QRegExp("\\*/");

    mCompleter = new QCompleter(this);
    completerStrings.sort(Qt::CaseInsensitive);
    auto completerModel = new QStringListModel(completerStrings, mCompleter);
    mCompleter->setModel(completerModel);
    mCompleter->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    mCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    mCompleter->setWrapAround(false);
}

void GlslHighlighter::highlightBlock(const QString &text)
{
    foreach (const HighlightingRule &rule, mHighlightingRules) {
        auto index = rule.pattern.indexIn(text);
        while (index >= 0) {
            const auto length = rule.pattern.matchedLength();
            setFormat(index, length, rule.format);
            index = rule.pattern.indexIn(text, index + length);
        }
    }
    setCurrentBlockState(0);

    auto startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = mCommentStartExpression.indexIn(text);

    while (startIndex >= 0) {
        const auto endIndex = mCommentEndExpression.indexIn(text, startIndex);
        auto commentLength = 0;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        }
        else {
            commentLength = endIndex - startIndex + mCommentEndExpression.matchedLength();
        }
        setFormat(startIndex, commentLength, mMultiLineCommentFormat);
        startIndex = mCommentStartExpression.indexIn(text, startIndex + commentLength);
    }
}
