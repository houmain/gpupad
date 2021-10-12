#include "HlslHighlighter.h"
#include <QCompleter>
#include <QStringListModel>

namespace {
const auto keywords = {
    "break", "continue", "if", "else", "switch", "return", "for", "while", "do", "typedef", "namespace", "true", "false", "compile", "discard", "inline",
    "const", "void", "struct", "static", "extern", "register", "volatile", "inline", "target", "nointerpolation", "shared", "uniform", "row_major", "column_major", "snorm", "unorm", "bool", "bool1", "bool2", "bool3", "bool4", "int", "int1", "int2", "int3", "int4", "uint", "uint1", "uint2", "uint3", "uint4", "half", "half1", "half2", "half3", "half4", "float", "float1", "float2", "float3", "float4", "double", "double1", "double2", "double3", "double4", "matrix", "bool1x1", "bool1x2", "bool1x3", "bool1x4", "bool2x1", "bool2x2", "bool2x3", "bool2x4", "bool3x1", "bool3x2", "bool3x3", "bool3x4", "bool4x1", "bool4x2", "bool4x3", "bool4x4", "int1x1", "int1x2", "int1x3", "int1x4", "int2x1", "int2x2", "int2x3", "int2x4", "int3x1", "int3x2", "int3x3", "int3x4", "int4x1", "int4x2", "int4x3", "int4x4", "uint1x1", "uint1x2", "uint1x3", "uint1x4", "uint2x1", "uint2x2", "uint2x3", "uint2x4", "uint3x1", "uint3x2", "uint3x3", "uint3x4", "uint4x1", "uint4x2", "uint4x3", "uint4x4", "half1x1", "half1x2", "half1x3", "half1x4", "half2x1", "half2x2", "half2x3", "half2x4", "half3x1", "half3x2", "half3x3", "half3x4", "half4x1", "half4x2", "half4x3", "half4x4", "float1x1", "float1x2", "float1x3", "float1x4", "float2x1", "float2x2", "float2x3", "float2x4", "float3x1", "float3x2", "float3x3", "float3x4", "float4x1", "float4x2", "float4x3", "float4x4", "double1x1", "double1x2", "double1x3", "double1x4", "double2x1", "double2x2", "double2x3", "double2x4", "double3x1", "double3x2", "double3x3", "double3x4", "double4x1", "double4x2", "double4x3", "double4x4", "cbuffer", "groupshared", "SamplerState", "in", "out", "inout", "vector", "matrix", "interface", "class", "point", "triangle", "line", "lineadj", "triangleadj",
    "Texture", "Texture1D", "Texture1DArray", "Texture2D", "Texture2DArray", "Texture2DMS", "Texture2DMSArray", "Texture3D", "TextureCube", "RWTexture1D", "RWTexture1DArray", "RWTexture2D", "RWTexture2DArray", "RWTexture3D", "Buffer", "StructuredBuffer", "AppendStructuredBuffer", "ConsumeStructuredBuffer", "RWBuffer", "RWStructuredBuffer", "ByteAddressBuffer", "RWByteAddressBuffer", "PointStream", "TriangleStream", "LineStream", "InputPatch", "OutputPatch"
};

const auto builtinFunctions = {
    "abs", "acos", "all", "AllMemoryBarrier", "AllMemoryBarrierWithGroupSync", "any", "asdouble", "asfloat", "asin", "asint", "asuint", "atan", "atan2", "ceil", "clamp", "clip", "cos", "cosh", "countbits", "cross", "ddx", "ddx_coarse", "ddx_fine", "ddy", "ddy_coards", "ddy_fine", "degrees", "determinant", "DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync", "distance", "dot", "dst", "EvaluateAttributeAtCentroid", "EvaluateAttributeAtSample", "EvaluateAttributeSnapped", "exp", "exp2", "f16tof32", "f32tof16", "faceforward", "firstbithigh", "firstbitlow", "floor", "fmod", "frac", "frexp", "fwidth", "GetRenderTargetSampleCount", "GetRenderTargetSamplePosition", "GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync", "InterlockedAdd", "InterlockedAnd", "InterlockedCompareExchange", "InterlockedExchange", "InterlockedMax", "InterlockedMin", "IntterlockedOr", "InterlockedXor", "isfinite", "isinf", "isnan", "ldexp", "length", "lerp", "lit", "log", "log10", "log2", "mad", "max", "min", "modf", "mul", "normalize", "pow", "Process2DQuadTessFactorsAvg", "Process2DQuadTessFactorsMax", "Process2DQuadTessFactorsMin", "ProcessIsolineTessFactors", "ProcessQuadTessFactorsAvg", "ProcessQuadTessFactorsMax", "ProcessQuadTessFactorsMin", "ProcessTriTessFactorsAvg", "ProcessTriTessFactorsMax", "ProcessTriTessFactorsMin", "radians", "rcp", "reflect", "refract", "reversebits", "round", "rsqrt", "saturate", "sign", "sin", "sincos", "sinh", "smoothstep", "sqrt", "step", "tan", "tanh", "transpose", "trunc",
    "Append", "RestartStrip", "CalculateLevelOfDetail", "CalculateLevelOfDetailUnclamped", "GetDimensions", "GetSamplePosition", "Load", "Sample", "SampleBias", "SampleCmp", "SampleCmpLevelZero", "SampleGrad", "SampleLevel", "Load2", "Load3", "Load4", "Consume", "Store", "Store2", "Store3", "Store4", "DecrementCounter", "IncrementCounter", "mips", "Gather", "GatherRed", "GatherGreen", "GatherBlue", "GatherAlpha", "GatherCmp", "GatherCmpRed", "GatherCmpGreen", "GatherCmpBlue", "GatherCmpAlpha" };

const auto layoutQualifiers = {
    "unroll", "loop", "flatten", "branch", "earlydepthstencil", "domain", "instance", "maxtessfactor", "outputcontrolpoints", "outputtopology", "partitioning", "patchconstantfunc", "numthreads", "maxvertexcount", "precise" };
} // namespace

HlslHighlighter::HlslHighlighter(bool darkTheme, QObject *parent)
    : QSyntaxHighlighter(parent)
{
    QTextCharFormat keywordFormat;
    QTextCharFormat builtinFunctionFormat;
    QTextCharFormat annotationFormat;
    QTextCharFormat preprocessorFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat whiteSpaceFormat;

    functionFormat.setFontWeight(QFont::Bold);

    if (darkTheme) {
        functionFormat.setForeground(QColor(0x7AAFFF));
        keywordFormat.setForeground(QColor(0x7AAFFF));
        builtinFunctionFormat.setForeground(QColor(0x7AAFFF));
        annotationFormat.setForeground(QColor(0xDD8D8D));
        numberFormat.setForeground(QColor(0xB09D30));
        quotationFormat.setForeground(QColor(0xB09D30));
        preprocessorFormat.setForeground(QColor(0xC87FFF));
        mCommentFormat.setForeground(QColor(0x56C056));
        whiteSpaceFormat.setForeground(QColor(0x666666));
    }
    else {
        functionFormat.setForeground(QColor(0x000066));        
        keywordFormat.setForeground(QColor(0x003C98));
        builtinFunctionFormat.setForeground(QColor(0x000066));
        annotationFormat.setForeground(QColor(0x981111));
        numberFormat.setForeground(QColor(0x981111));
        quotationFormat.setForeground(QColor(0x981111));
        preprocessorFormat.setForeground(QColor(0x800080));
        mCommentFormat.setForeground(QColor(0x008700));
        whiteSpaceFormat.setForeground(QColor(0xCCCCCC));
    }

    auto rule = HighlightingRule();
    auto completerStrings = QStringList();

    for (const auto &keyword : keywords) {
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(keyword));
        rule.format = keywordFormat;
        mHighlightingRules.append(rule);
        completerStrings.append(keyword);
    }

    for (const auto &builtinFunction : builtinFunctions) {
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(builtinFunction));
        rule.format = builtinFunctionFormat;
        mHighlightingRules.append(rule);
        completerStrings.append(builtinFunction);
    }

    for (const auto &qaulifier : layoutQualifiers)
        completerStrings.append(qaulifier);

    rule.pattern = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()");
    rule.format = functionFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegularExpression(":\\s*[A-Za-z_][A-Za-z0-9_]*");
    rule.format = annotationFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegularExpression("\\b[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?[uUlLfF]{,2}\\b");
    rule.format = numberFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegularExpression("\\b0x[0-9,A-F,a-f]+[uUlL]\\b");
    rule.format = numberFormat;
    mHighlightingRules.append(rule);

    auto quotation = QString("%1([^%1]*(\\\\%1[^%1]*)*)(%1|$)");
    rule.pattern = QRegularExpression(quotation.arg('"'));
    rule.format = quotationFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegularExpression("^\\s*#.*");
    rule.format = preprocessorFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegularExpression("//.*");
    rule.format = mCommentFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegularExpression("\\s+", QRegularExpression::UseUnicodePropertiesOption);
    rule.format = whiteSpaceFormat;
    mHighlightingRules.append(rule);

    mCommentStartExpression = QRegularExpression("/\\*");
    mCommentEndExpression = QRegularExpression("\\*/");

    mCompleter = new QCompleter(this);
    completerStrings.sort(Qt::CaseInsensitive);
    auto completerModel = new QStringListModel(completerStrings, mCompleter);
    mCompleter->setModel(completerModel);
    mCompleter->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    mCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    mCompleter->setWrapAround(false);
}

void HlslHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : qAsConst(mHighlightingRules)) {
        auto index = text.indexOf(rule.pattern);
        while (index >= 0) {
            const auto match = rule.pattern.match(text, index);
            const auto length = match.capturedLength();
            // do not start single line comment in string
            if (format(index) == QTextCharFormat{ })
                setFormat(index, length, rule.format);
            index = text.indexOf(rule.pattern, index + length);
        }
    }
    setCurrentBlockState(0);

    auto startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(mCommentStartExpression);

    while (startIndex >= 0) {
        // do not start multiline comment in single line comment or string
        if (startIndex && format(startIndex) != QTextCharFormat{ }) {
            setCurrentBlockState(0);
            break;
        }

        const auto match = mCommentEndExpression.match(text, startIndex);
        const auto endIndex = match.capturedStart();
        auto commentLength = 0;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        }
        else {
            commentLength = endIndex - startIndex + match.capturedLength();
        }
        setFormat(startIndex, commentLength, mCommentFormat);
        startIndex = text.indexOf(mCommentStartExpression, startIndex + commentLength);
    }
}
