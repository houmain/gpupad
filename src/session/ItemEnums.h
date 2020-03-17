#ifndef ITEMENUMS_H
#define ITEMENUMS_H

#include <QObject>
#include <QOpenGLTexture>

namespace ItemEnums {
    Q_NAMESPACE

    enum class ItemType
    {
        Group,
        Buffer,
        Column,
        Texture,
        Image,
        Program,
        Shader,
        Binding,
        Stream,
        Attribute,
        Target,
        Attachment,
        Call,
        Script,
    };
    Q_ENUM_NS(ItemType)

    enum DataType {
        Int8 = GL_BYTE,
        Int16 = GL_SHORT,
        Int32 = GL_INT,
        //Int64 = GL_INT64,
        Uint8 = GL_UNSIGNED_BYTE,
        Uint16 = GL_UNSIGNED_SHORT,
        Uint32 = GL_UNSIGNED_INT,
        //Uint64 = GL_UNSIGNED_INT64,
        Float = GL_FLOAT,
        Double = GL_DOUBLE,
    };
    Q_ENUM_NS(DataType)

    enum ShaderType {
        Header = 0,
        Vertex = GL_VERTEX_SHADER,
        Fragment = GL_FRAGMENT_SHADER,
        Geometry = GL_GEOMETRY_SHADER,
        TessellationControl = GL_TESS_CONTROL_SHADER,
        TessellationEvaluation = GL_TESS_EVALUATION_SHADER,
        Compute = GL_COMPUTE_SHADER,
    };
    Q_ENUM_NS(ShaderType)

    enum BindingType {
        Uniform,
        Sampler,
        Image,
        Buffer,
        Subroutine,
    };
    Q_ENUM_NS(BindingType)

    enum ComparisonFunc {
        NoComparisonFunc = GL_NONE,
        LessEqual = GL_LEQUAL,
        GreaterEqual = GL_GEQUAL,
        Less = GL_LESS,
        Greater = GL_GREATER,
        Equal = GL_EQUAL,
        NotEqual = GL_NOTEQUAL,
        Always = GL_ALWAYS,
        Never = GL_NEVER,
    };
    Q_ENUM_NS(ComparisonFunc)

    enum BindingEditor {
        Expression, Expression2, Expression3, Expression4,
        Expression2x2, Expression2x3, Expression2x4,
        Expression3x2, Expression3x3, Expression3x4,
        Expression4x2, Expression4x3, Expression4x4,
        Color
    };
    Q_ENUM_NS(BindingEditor)

    enum FrontFace {
        CCW = GL_CCW,
        CW = GL_CW,
    };
    Q_ENUM_NS(FrontFace)

    enum CullMode {
        NoCulling = GL_NONE,
        Back = GL_BACK,
        Front = GL_FRONT,
        FrontAndBack = GL_FRONT_AND_BACK
    };
    Q_ENUM_NS(CullMode)

    enum LogicOperation {
        NoLogicOperation = GL_NONE,
        Copy = GL_COPY,
        Clear = GL_CLEAR,
        Set = GL_SET,
        CopyInverted = GL_COPY_INVERTED,
        NoOp = GL_NOOP,
        Invert = GL_INVERT,
        And = GL_AND,
        Nand = GL_NAND,
        Or = GL_OR,
        Nor = GL_NOR,
        Xor = GL_XOR,
        Equiv = GL_EQUIV,
        AndReverse = GL_AND_REVERSE,
        AndInverted = GL_AND_INVERTED,
        OrReverse = GL_OR_REVERSE,
        OrInverted = GL_OR_INVERTED,
    };
    Q_ENUM_NS(LogicOperation)

    enum BlendEquation {
        Add = GL_FUNC_ADD,
        Min = GL_MIN,
        Max = GL_MAX,
        Subtract = GL_FUNC_SUBTRACT,
        ReverseSubtract = GL_FUNC_REVERSE_SUBTRACT,
    };
    Q_ENUM_NS(BlendEquation)

    enum BlendFactor {
        Zero = GL_ZERO,
        One = GL_ONE,
        SrcColor = GL_SRC_COLOR,
        OneMinusSrcColor = GL_ONE_MINUS_SRC_COLOR,
        SrcAlpha = GL_SRC_ALPHA,
        OneMinusSrcAlpha = GL_ONE_MINUS_SRC_ALPHA,
        DstAlpha = GL_DST_ALPHA,
        OneMinusDstAlpha = GL_ONE_MINUS_DST_ALPHA,
        DstColor = GL_DST_COLOR,
        OneMinusDstColor = GL_ONE_MINUS_DST_COLOR,
        SrcAlphaSaturate = GL_SRC_ALPHA_SATURATE,
        ConstantColor = GL_CONSTANT_COLOR,
        OneMinusConstantColor = GL_ONE_MINUS_CONSTANT_COLOR,
        ConstantAlpha = GL_CONSTANT_ALPHA,
        OneMinusConstantAlpha = GL_ONE_MINUS_CONSTANT_ALPHA,
    };
    Q_ENUM_NS(BlendFactor)

    enum PrimitiveType {
        Points = GL_POINTS,
        LineStrip = GL_LINE_STRIP,
        LineLoop = GL_LINE_LOOP,
        Lines = GL_LINES,
        LineStripAdjacency = GL_LINE_STRIP_ADJACENCY,
        LinesAdjacency = GL_LINES_ADJACENCY,
        TriangleStrip = GL_TRIANGLE_STRIP,
        TriangleFan = GL_TRIANGLE_FAN,
        Triangles = GL_TRIANGLES,
        TriangleStripAdjacency = GL_TRIANGLE_STRIP_ADJACENCY,
        TrianglesAdjacency = GL_TRIANGLES_ADJACENCY,
        Patches = GL_PATCHES
    };
    Q_ENUM_NS(PrimitiveType)

    enum ExecuteOn {
        ResetEvaluation,
        ManualEvaluation,
        EveryEvaluation,
    };
    Q_ENUM_NS(ExecuteOn)
}

// enums moved to another namespace because of name clashes
namespace ItemEnums2 {
    Q_NAMESPACE

    enum CallType {
        Draw, // glDrawArrays(InstancedBaseInstance)
        DrawIndexed, // glDrawElements(InstancedBaseVertexBaseInstance)
        DrawIndirect, // gl(Multi)DrawArraysIndirect)
        DrawIndexedIndirect, // gl(Multi)DrawElementsIndirect
        Compute,
        ClearTexture,
        CopyTexture,
        ClearBuffer,
        CopyBuffer,
    };
    Q_ENUM_NS(CallType)

    enum StencilOperation {
        Keep = GL_KEEP,
        Zero = GL_ZERO,
        Replace = GL_REPLACE,
        Increment = GL_INCR,
        IncrementWrap = GL_INCR_WRAP,
        Decrement = GL_DECR,
        DecrementWrap = GL_DECR_WRAP,
        Invert = GL_INVERT,
    };
    Q_ENUM_NS(StencilOperation)
}

#endif // ITEMENUMS_H
