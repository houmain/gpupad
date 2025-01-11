#pragma once

#include <QString>

enum class SourceType {
    PlainText,
    Generic,
    GLSL_VertexShader,
    GLSL_FragmentShader,
    GLSL_GeometryShader,
    GLSL_TessControlShader,
    GLSL_TessEvaluationShader,
    GLSL_ComputeShader,
    GLSL_TaskShader,
    GLSL_MeshShader,
    GLSL_RayGenerationShader,
    GLSL_RayIntersectionShader,
    GLSL_RayAnyHitShader,
    GLSL_RayClosestHitShader,
    GLSL_RayMissShader,
    GLSL_RayCallableShader,
    HLSL_VertexShader,
    HLSL_PixelShader,
    HLSL_GeometryShader,
    HLSL_HullShader,
    HLSL_DomainShader,
    HLSL_ComputeShader,
    HLSL_AmplificationShader,
    HLSL_MeshShader,
    HLSL_RayGenerationShader,
    HLSL_RayIntersectionShader,
    HLSL_RayAnyHitShader,
    HLSL_RayClosestHitShader,
    HLSL_RayMissShader,
    HLSL_RayCallableShader,
    JavaScript,
};

SourceType deduceSourceType(SourceType current, const QString &extension,
    const QString &text);
