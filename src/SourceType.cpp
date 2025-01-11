
#include "session/Item.h"

SourceType deduceSourceType(SourceType current, const QString &extension,
    const QString &source)
{
    if (extension == "vs" || extension == "vert")
        return SourceType::GLSL_VertexShader;
    if (extension == "fs" || extension == "frag")
        return SourceType::GLSL_FragmentShader;
    if (extension == "gs" || extension == "geom")
        return SourceType::GLSL_GeometryShader;
    if (extension == "tesc")
        return SourceType::GLSL_TessControlShader;
    if (extension == "tese")
        return SourceType::GLSL_TessEvaluationShader;
    if (extension == "comp")
        return SourceType::GLSL_ComputeShader;
    if (extension == "mesh")
        return SourceType::GLSL_MeshShader;
    if (extension == "task")
        return SourceType::GLSL_TaskShader;
    if (extension == "rgen")
        return SourceType::GLSL_RayGenerationShader;
    if (extension == "rint")
        return SourceType::GLSL_RayIntersectionShader;
    if (extension == "rahit")
        return SourceType::GLSL_RayAnyHitShader;
    if (extension == "rchit")
        return SourceType::GLSL_RayClosestHitShader;
    if (extension == "rmiss")
        return SourceType::GLSL_RayMissShader;
    if (extension == "rcall")
        return SourceType::GLSL_RayCallableShader;

    if (extension == "js" || extension == "json" || extension == "qml")
        return SourceType::JavaScript;
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
    using SL = Shader::Language;
    using ST = Shader::ShaderType;

    static const auto sMapping = QMap<std::pair<SL, ST>, SourceType>{
        { { SL::GLSL, ST::Vertex }, SourceType::GLSL_VertexShader },
        { { SL::GLSL, ST::Fragment }, SourceType::GLSL_FragmentShader },
        { { SL::GLSL, ST::Geometry }, SourceType::GLSL_GeometryShader },
        { { SL::GLSL, ST::TessControl }, SourceType::GLSL_TessControlShader },
        { { SL::GLSL, ST::TessEvaluation },
            SourceType::GLSL_TessEvaluationShader },
        { { SL::GLSL, ST::Compute }, SourceType::GLSL_ComputeShader },
        { { SL::GLSL, ST::Task }, SourceType::GLSL_TaskShader },
        { { SL::GLSL, ST::Mesh }, SourceType::GLSL_MeshShader },
        { { SL::GLSL, ST::RayGeneration },
            SourceType::GLSL_RayGenerationShader },
        { { SL::GLSL, ST::RayIntersection },
            SourceType::GLSL_RayIntersectionShader },
        { { SL::GLSL, ST::RayAnyHit }, SourceType::GLSL_RayAnyHitShader },
        { { SL::GLSL, ST::RayClosestHit },
            SourceType::GLSL_RayClosestHitShader },
        { { SL::GLSL, ST::RayMiss }, SourceType::GLSL_RayMissShader },
        { { SL::GLSL, ST::RayCallable }, SourceType::GLSL_RayCallableShader },
        { { SL::HLSL, ST::Vertex }, SourceType::HLSL_VertexShader },
        { { SL::HLSL, ST::Fragment }, SourceType::HLSL_PixelShader },
        { { SL::HLSL, ST::Geometry }, SourceType::HLSL_GeometryShader },
        { { SL::HLSL, ST::TessControl }, SourceType::HLSL_HullShader },
        { { SL::HLSL, ST::TessEvaluation }, SourceType::HLSL_DomainShader },
        { { SL::HLSL, ST::Compute }, SourceType::HLSL_ComputeShader },
        { { SL::HLSL, ST::Task }, SourceType::HLSL_AmplificationShader },
        { { SL::HLSL, ST::Mesh }, SourceType::HLSL_MeshShader },
        { { SL::HLSL, ST::RayGeneration },
            SourceType::HLSL_RayGenerationShader },
        { { SL::HLSL, ST::RayIntersection },
            SourceType::HLSL_RayIntersectionShader },
        { { SL::HLSL, ST::RayAnyHit }, SourceType::HLSL_RayAnyHitShader },
        { { SL::HLSL, ST::RayClosestHit },
            SourceType::HLSL_RayClosestHitShader },
        { { SL::HLSL, ST::RayMiss }, SourceType::HLSL_RayMissShader },
        { { SL::HLSL, ST::RayCallable }, SourceType::HLSL_RayCallableShader },
    };
    return sMapping[{ language, type }];
}

Shader::ShaderType getShaderType(SourceType sourceType)
{
    using ST = Shader::ShaderType;

    switch (sourceType) {
    case SourceType::PlainText:
    case SourceType::Generic:
    case SourceType::JavaScript:                 break;
    case SourceType::GLSL_VertexShader:
    case SourceType::HLSL_VertexShader:          return ST::Vertex;
    case SourceType::GLSL_FragmentShader:
    case SourceType::HLSL_PixelShader:           return ST::Fragment;
    case SourceType::GLSL_GeometryShader:
    case SourceType::HLSL_GeometryShader:        return ST::Geometry;
    case SourceType::GLSL_TessControlShader:
    case SourceType::HLSL_HullShader:            return ST::TessControl;
    case SourceType::GLSL_TessEvaluationShader:
    case SourceType::HLSL_DomainShader:          return ST::TessEvaluation;
    case SourceType::GLSL_ComputeShader:
    case SourceType::HLSL_ComputeShader:         return ST::Compute;
    case SourceType::GLSL_TaskShader:
    case SourceType::HLSL_AmplificationShader:   return ST::Task;
    case SourceType::GLSL_MeshShader:
    case SourceType::HLSL_MeshShader:            return ST::Mesh;
    case SourceType::GLSL_RayGenerationShader:
    case SourceType::HLSL_RayGenerationShader:   return ST::RayGeneration;
    case SourceType::GLSL_RayIntersectionShader:
    case SourceType::HLSL_RayIntersectionShader: return ST::RayIntersection;
    case SourceType::GLSL_RayAnyHitShader:
    case SourceType::HLSL_RayAnyHitShader:       return ST::RayAnyHit;
    case SourceType::GLSL_RayClosestHitShader:
    case SourceType::HLSL_RayClosestHitShader:   return ST::RayClosestHit;
    case SourceType::GLSL_RayMissShader:
    case SourceType::HLSL_RayMissShader:         return ST::RayMiss;
    case SourceType::GLSL_RayCallableShader:
    case SourceType::HLSL_RayCallableShader:     return ST::RayCallable;
    }
    return {};
}

Shader::Language getShaderLanguage(SourceType sourceType)
{
    switch (sourceType) {
    case SourceType::PlainText:
    case SourceType::Generic:
    case SourceType::JavaScript: break;

    case SourceType::GLSL_VertexShader:
    case SourceType::GLSL_FragmentShader:
    case SourceType::GLSL_GeometryShader:
    case SourceType::GLSL_TessControlShader:
    case SourceType::GLSL_TessEvaluationShader:
    case SourceType::GLSL_ComputeShader:
    case SourceType::GLSL_TaskShader:
    case SourceType::GLSL_MeshShader:
    case SourceType::GLSL_RayGenerationShader:
    case SourceType::GLSL_RayIntersectionShader:
    case SourceType::GLSL_RayAnyHitShader:
    case SourceType::GLSL_RayClosestHitShader:
    case SourceType::GLSL_RayMissShader:
    case SourceType::GLSL_RayCallableShader:     return Shader::Language::GLSL;

    case SourceType::HLSL_VertexShader:
    case SourceType::HLSL_PixelShader:
    case SourceType::HLSL_GeometryShader:
    case SourceType::HLSL_HullShader:
    case SourceType::HLSL_DomainShader:
    case SourceType::HLSL_ComputeShader:
    case SourceType::HLSL_AmplificationShader:
    case SourceType::HLSL_MeshShader:
    case SourceType::HLSL_RayGenerationShader:
    case SourceType::HLSL_RayIntersectionShader:
    case SourceType::HLSL_RayAnyHitShader:
    case SourceType::HLSL_RayClosestHitShader:
    case SourceType::HLSL_RayMissShader:
    case SourceType::HLSL_RayCallableShader:     return Shader::Language::HLSL;
    }
    return Shader::Language::None;
}
