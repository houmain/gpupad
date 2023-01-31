
#include "session/Item.h"
#include "SourceType.h"

SourceType deduceSourceType(SourceType current, const QString &extension, const QString &source)
{
    if (extension == "vs" || extension == "vert")
        return SourceType::GLSL_VertexShader;
    if (extension == "fs" || extension == "frag")
        return SourceType::GLSL_FragmentShader;
    if (extension == "gs" || extension == "geom")
        return SourceType::GLSL_GeometryShader;
    if (extension == "tesc")
        return SourceType::GLSL_TessellationControl;
    if (extension == "tese")
        return SourceType::GLSL_TessellationEvaluation;
    if (extension == "comp")
        return SourceType::GLSL_ComputeShader;
    if (extension == "js" || extension == "json" || extension == "qml")
        return SourceType::JavaScript;
    if (extension == "lua")
        return SourceType::Lua;
    if (extension == "ps")
        return SourceType::HLSL_PixelShader;

    if (extension == "glsl") {
        if (source.contains("gl_Position"))
            return SourceType::GLSL_VertexShader;
        if (source.contains("local_size_x"))
            return SourceType::GLSL_ComputeShader;
        if (getShaderLanguage(current) != Shader::Language::GLSL)
            return SourceType::GLSL_FragmentShader;
        return current;
    }

    if (extension == "hlsl" || extension == "hlsli" || extension == "fx") {
        if (getShaderLanguage(current) != Shader::Language::HLSL) {
            if (source.contains("PS"))
                return SourceType::HLSL_PixelShader;
            if (source.contains("VS"))
                return SourceType::HLSL_VertexShader;
            if (source.contains("CS"))
                return SourceType::HLSL_ComputeShader;
            if (source.contains("GS"))
                return SourceType::HLSL_GeometryShader;
            if (source.contains("HS"))
                return SourceType::HLSL_HullShader;
            if (source.contains("DS"))
                return SourceType::HLSL_DomainShader;
            return SourceType::HLSL_PixelShader;
        }
        return current;
    }

    if (extension == "txt" || extension == "log" || extension == "csv")
        return SourceType::PlainText;

    if (extension == "cfg" || extension == "conf" || source.startsWith("#!"))
        return SourceType::Generic;

    return current;
}

SourceType getSourceType(Shader::ShaderType type, Shader::Language language)
{
    static const auto sMapping = QMap<std::pair<Shader::Language, Shader::ShaderType>, SourceType>{
        { { Shader::Language::GLSL, Shader::ShaderType::Vertex }, SourceType::GLSL_VertexShader },
        { { Shader::Language::GLSL, Shader::ShaderType::Fragment }, SourceType::GLSL_FragmentShader },
        { { Shader::Language::GLSL, Shader::ShaderType::Geometry }, SourceType::GLSL_GeometryShader },
        { { Shader::Language::GLSL, Shader::ShaderType::TessellationControl }, SourceType::GLSL_TessellationControl },
        { { Shader::Language::GLSL, Shader::ShaderType::TessellationEvaluation }, SourceType::GLSL_TessellationEvaluation },
        { { Shader::Language::GLSL, Shader::ShaderType::Compute }, SourceType::GLSL_ComputeShader },

        { { Shader::Language::HLSL, Shader::ShaderType::Vertex }, SourceType::HLSL_VertexShader },
        { { Shader::Language::HLSL, Shader::ShaderType::Fragment }, SourceType::HLSL_PixelShader },
        { { Shader::Language::HLSL, Shader::ShaderType::Geometry }, SourceType::HLSL_GeometryShader },
        { { Shader::Language::HLSL, Shader::ShaderType::TessellationControl }, SourceType::HLSL_HullShader },
        { { Shader::Language::HLSL, Shader::ShaderType::TessellationEvaluation }, SourceType::HLSL_DomainShader },
        { { Shader::Language::HLSL, Shader::ShaderType::Compute }, SourceType::HLSL_ComputeShader },
    };
    return sMapping[{ language, type }];
}

Shader::ShaderType getShaderType(SourceType sourceType)
{
    switch (sourceType) {
        case SourceType::PlainText:
        case SourceType::Generic:
        case SourceType::JavaScript:
        case SourceType::Lua:
            break;

        case SourceType::GLSL_VertexShader:
        case SourceType::HLSL_VertexShader:
            return Shader::ShaderType::Vertex;

        case SourceType::GLSL_FragmentShader:
        case SourceType::HLSL_PixelShader:
            return Shader::ShaderType::Fragment;

        case SourceType::GLSL_GeometryShader:
        case SourceType::HLSL_GeometryShader:
            return Shader::ShaderType::Geometry;

        case SourceType::GLSL_TessellationControl:
        case SourceType::HLSL_HullShader:
            return Shader::ShaderType::TessellationControl;

        case SourceType::GLSL_TessellationEvaluation:
        case SourceType::HLSL_DomainShader:
            return Shader::ShaderType::TessellationEvaluation;

        case SourceType::GLSL_ComputeShader:
        case SourceType::HLSL_ComputeShader:
            return Shader::ShaderType::Compute;
    }
    return { };
}

Shader::Language getShaderLanguage(SourceType sourceType)
{
    switch (sourceType) {
        case SourceType::PlainText:
        case SourceType::Generic:
        case SourceType::JavaScript:
        case SourceType::Lua:
            break;

        case SourceType::GLSL_VertexShader:
        case SourceType::GLSL_FragmentShader:
        case SourceType::GLSL_GeometryShader:
        case SourceType::GLSL_TessellationControl:
        case SourceType::GLSL_TessellationEvaluation:
        case SourceType::GLSL_ComputeShader:
            return Shader::Language::GLSL;

        case SourceType::HLSL_VertexShader:
        case SourceType::HLSL_PixelShader:
        case SourceType::HLSL_GeometryShader:
        case SourceType::HLSL_HullShader:
        case SourceType::HLSL_DomainShader:
        case SourceType::HLSL_ComputeShader:
            return Shader::Language::HLSL;
    }
    return Shader::Language::None;
}
