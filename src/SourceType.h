#ifndef SOURCETYPE_H
#define SOURCETYPE_H

enum class SourceType
{
    None,
    PlainText,
    VertexShader,
    FragmentShader,
    GeometryShader,
    TesselationControl,
    TesselationEvaluation,
    ComputeShader,
    JavaScript,
};

#endif // SOURCETYPE_H
