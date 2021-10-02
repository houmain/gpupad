#ifndef SOURCETYPE_H
#define SOURCETYPE_H

#include <QString>

enum class SourceType
{
    PlainText,
    GLSL_VertexShader,
    GLSL_FragmentShader,
    GLSL_GeometryShader,
    GLSL_TessellationControl,
    GLSL_TessellationEvaluation,
    GLSL_ComputeShader,
    HLSL_VertexShader,
    HLSL_FragmentShader,
    HLSL_GeometryShader,
    HLSL_TessellationControl,
    HLSL_TessellationEvaluation,
    HLSL_ComputeShader,
    JavaScript,
};

SourceType deduceSourceType(SourceType current, const QString &extension, const QString &text);

#ifdef ITEM_H
SourceType getSourceType(Shader::ShaderType type, Shader::Language language);
Shader::ShaderType getShaderType(SourceType sourceType);
Shader::Language getShaderLanguage(SourceType sourceType);
#endif

#endif // SOURCETYPE_H
