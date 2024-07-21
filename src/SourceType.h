#pragma once

#include <QString>

enum class SourceType
{
    PlainText,
    Generic,
    GLSL_VertexShader,
    GLSL_FragmentShader,
    GLSL_GeometryShader,
    GLSL_TessellationControl,
    GLSL_TessellationEvaluation,
    GLSL_ComputeShader,
    HLSL_VertexShader,
    HLSL_PixelShader,
    HLSL_GeometryShader,
    HLSL_HullShader,
    HLSL_DomainShader,
    HLSL_ComputeShader,
    JavaScript,
};

SourceType deduceSourceType(SourceType current, const QString &extension, const QString &text);

#ifdef ITEM_H
SourceType getSourceType(Shader::ShaderType type, Shader::Language language);
Shader::ShaderType getShaderType(SourceType sourceType);
Shader::Language getShaderLanguage(SourceType sourceType);
#endif

