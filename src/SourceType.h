#ifndef SOURCETYPE_H
#define SOURCETYPE_H

enum class SourceType
{
    None,
    PlainText,
    VertexShader,
    FragmentShader,
    GeometryShader,
    TessellationControl,
    TessellationEvaluation,
    ComputeShader,
    JavaScript,
};

#endif // SOURCETYPE_H
