#ifndef ITEMENUMS_H
#define ITEMENUMS_H

#include <QObject>
#include <QOpenGLTexture>

#if !defined(GL_COMPUTE_SHADER)
# define GL_COMPUTE_SHADER 0x91B9
#endif

namespace ItemEnums {
    Q_NAMESPACE

    enum class ItemType
    {
        Group,
        Buffer,
        Block,
        Field,
        Texture,
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
        Vertex = GL_VERTEX_SHADER,
        Fragment = GL_FRAGMENT_SHADER,
        Geometry = GL_GEOMETRY_SHADER,
        TessellationControl = GL_TESS_CONTROL_SHADER,
        TessellationEvaluation = GL_TESS_EVALUATION_SHADER,
        Compute = GL_COMPUTE_SHADER,
        Includable = 0,
    };
    Q_ENUM_NS(ShaderType)

    enum ShaderLanguage {
        GLSL,
        HLSL,
        None,
    };
    Q_ENUM_NS(ShaderLanguage)

    enum BindingType {
        Uniform,
        Sampler,
        Buffer,
        BufferBlock,
        Image,
        TextureBuffer,
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

    enum ImageBindingFormat {
        Internal       = GL_NONE,

        // 8 bit
        r8             = GL_R8,
        r8ui           = GL_R8UI,
        r8i            = GL_R8I,

        // 16 bit
        r16            = GL_R16,
        r16_snorm      = GL_R16_SNORM, // not supported by glTexBuffer
        r16f           = GL_R16F,
        r16ui          = GL_R16UI,
        r16i           = GL_R16I,
        rg8            = GL_RG8,
        rg8_snorm      = GL_RG8_SNORM, // not supported by glTexBuffer
        rg8ui          = GL_RG8UI,
        rg8i           = GL_RG8I,

        // 24 bit - not supported by glBindImageTexture
        rgb32f         = GL_RGB32F,
        rgb32i         = GL_RGB32I,
        rgb32ui        = GL_RGB32UI,

        // 32 bit
        r32f           = GL_R32F,
        r32ui          = GL_R32UI,
        r32i           = GL_R32I,
        rg16           = GL_RG16,
        rg16_snorm     = GL_RG16_SNORM, // not supported by glTexBuffer
        rg16ui         = GL_RG16UI,
        rg16i          = GL_RG16I,
        rg16f          = GL_RG16F,
        rgba8          = GL_RGBA8,
        rgba8_snorm    = GL_RGBA8_SNORM, // not supported by glTexBuffer
        rgba8ui        = GL_RGBA8UI,
        rgba8i         = GL_RGBA8I,
        rgb10_a2       = GL_RGB10_A2, // not supported by glTexBuffer
        rgb10_a2ui     = GL_RGB10_A2UI, // not supported by glTexBuffer
        r11f_g11f_b10f = GL_R11F_G11F_B10F, // not supported by glTexBuffer

        // 64 bit
        rg32f          = GL_RG32F,
        rg32ui         = GL_RG32UI,
        rg32i          = GL_RG32I,
        rgba16         = GL_RGBA16,
        rgba16_snorm   = GL_RGBA16_SNORM, // not supported by glTexBuffer
        rgba16f        = GL_RGBA16F,
        rgba16ui       = GL_RGBA16UI,
        rgba16i        = GL_RGBA16I,

        // 128 bit
        rgba32f        = GL_RGBA32F,
        rgba32i        = GL_RGBA32I,
        rgba32ui       = GL_RGBA32UI,
    };
    Q_ENUM_NS(ImageBindingFormat)

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

    enum PolygonMode {
        Fill = GL_FILL,
        Line = GL_LINE,
        Point = GL_POINT,
    };
    Q_ENUM_NS(PolygonMode)

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
        //LineLoop = GL_LINE_LOOP,
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
        ComputeIndirect,
        ClearTexture,
        CopyTexture,
        SwapTextures,
        ClearBuffer,
        CopyBuffer,
        SwapBuffers,

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
