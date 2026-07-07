#pragma once

#include <QObject>

namespace ItemEnums {
    Q_NAMESPACE

    enum class ItemType {
        Root,
        Session,
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
        AccelerationStructure,
        Instance,
        Geometry,
    };
    Q_ENUM_NS(ItemType)

    enum TextureTarget {
        Target1D = 0x0DE0,
        Target1DArray = 0x8C18,
        Target2D = 0x0DE1,
        Target2DArray = 0x8C1A,
        Target3D = 0x806F,
        TargetCubeMap = 0x8513,
        TargetCubeMapArray = 0x9009,
        Target2DMultisample = 0x9100,
        Target2DMultisampleArray = 0x9102,
        TargetRectangle = 0x84F5,
        TargetBuffer = 0x8C2A,
    };
    Q_ENUM_NS(TextureTarget)

    enum TextureFormat {
        NoFormat = 0,

        R8_UNorm = 0x8229,
        RG8_UNorm = 0x822B,
        RGB8_UNorm = 0x8051,
        RGBA8_UNorm = 0x8058,
        R16_UNorm = 0x822A,
        RG16_UNorm = 0x822C,
        RGB16_UNorm = 0x8054,
        RGBA16_UNorm = 0x805B,

        R8_SNorm = 0x8F94,
        RG8_SNorm = 0x8F95,
        RGB8_SNorm = 0x8F96,
        RGBA8_SNorm = 0x8F97,
        R16_SNorm = 0x8F98,
        RG16_SNorm = 0x8F99,
        RGB16_SNorm = 0x8F9A,
        RGBA16_SNorm = 0x8F9B,

        R8U = 0x8232,
        RG8U = 0x8238,
        RGB8U = 0x8D7D,
        RGBA8U = 0x8D7C,
        R16U = 0x8234,
        RG16U = 0x823A,
        RGB16U = 0x8D77,
        RGBA16U = 0x8D76,
        R32U = 0x8236,
        RG32U = 0x823C,
        RGB32U = 0x8D71,
        RGBA32U = 0x8D70,

        R8I = 0x8231,
        RG8I = 0x8237,
        RGB8I = 0x8D8F,
        RGBA8I = 0x8D8E,
        R16I = 0x8233,
        RG16I = 0x8239,
        RGB16I = 0x8D89,
        RGBA16I = 0x8D88,
        R32I = 0x8235,
        RG32I = 0x823B,
        RGB32I = 0x8D83,
        RGBA32I = 0x8D82,

        R16F = 0x822D,
        RG16F = 0x822F,
        RGB16F = 0x881B,
        RGBA16F = 0x881A,
        R32F = 0x822E,
        RG32F = 0x8230,
        RGB32F = 0x8815,
        RGBA32F = 0x8814,

        RGB9E5 = 0x8C3D,
        RG11B10F = 0x8C3A,
        RG3B2 = 0x2A10,
        R5G6B5 = 0x8D62,
        RGB5A1 = 0x8057,
        RGBA4 = 0x8056,
        RGB10A2 = 0x906F,

        D16 = 0x81A5,
        D24 = 0x81A6,
        D24S8 = 0x88F0,
        D32 = 0x81A7,
        D32F = 0x8CAC,
        D32FS8X24 = 0x8CAD,
        S8 = 0x8D48,

        RGB_DXT1 = 0x83F0,
        RGBA_DXT1 = 0x83F1,
        RGBA_DXT3 = 0x83F2,
        RGBA_DXT5 = 0x83F3,
        R_ATI1N_UNorm = 0x8DBB,
        R_ATI1N_SNorm = 0x8DBC,
        RG_ATI2N_UNorm = 0x8DBD,
        RG_ATI2N_SNorm = 0x8DBE,
        RGB_BP_UNSIGNED_FLOAT = 0x8E8F,
        RGB_BP_SIGNED_FLOAT = 0x8E8E,
        RGB_BP_UNorm = 0x8E8C,
        R11_EAC_UNorm = 0x9270,
        R11_EAC_SNorm = 0x9271,
        RG11_EAC_UNorm = 0x9272,
        RG11_EAC_SNorm = 0x9273,
        RGB8_ETC2 = 0x9274,
        SRGB8_ETC2 = 0x9275,
        RGB8_PunchThrough_Alpha1_ETC2 = 0x9276,
        SRGB8_PunchThrough_Alpha1_ETC2 = 0x9277,
        RGBA8_ETC2_EAC = 0x9278,
        SRGB8_Alpha8_ETC2_EAC = 0x9279,
        RGB8_ETC1 = 0x8D64,
        RGBA_ASTC_4x4 = 0x93B0,
        RGBA_ASTC_5x4 = 0x93B1,
        RGBA_ASTC_5x5 = 0x93B2,
        RGBA_ASTC_6x5 = 0x93B3,
        RGBA_ASTC_6x6 = 0x93B4,
        RGBA_ASTC_8x5 = 0x93B5,
        RGBA_ASTC_8x6 = 0x93B6,
        RGBA_ASTC_8x8 = 0x93B7,
        RGBA_ASTC_10x5 = 0x93B8,
        RGBA_ASTC_10x6 = 0x93B9,
        RGBA_ASTC_10x8 = 0x93BA,
        RGBA_ASTC_10x10 = 0x93BB,
        RGBA_ASTC_12x10 = 0x93BC,
        RGBA_ASTC_12x12 = 0x93BD,
        SRGB8_Alpha8_ASTC_4x4 = 0x93D0,
        SRGB8_Alpha8_ASTC_5x4 = 0x93D1,
        SRGB8_Alpha8_ASTC_5x5 = 0x93D2,
        SRGB8_Alpha8_ASTC_6x5 = 0x93D3,
        SRGB8_Alpha8_ASTC_6x6 = 0x93D4,
        SRGB8_Alpha8_ASTC_8x5 = 0x93D5,
        SRGB8_Alpha8_ASTC_8x6 = 0x93D6,
        SRGB8_Alpha8_ASTC_8x8 = 0x93D7,
        SRGB8_Alpha8_ASTC_10x5 = 0x93D8,
        SRGB8_Alpha8_ASTC_10x6 = 0x93D9,
        SRGB8_Alpha8_ASTC_10x8 = 0x93DA,
        SRGB8_Alpha8_ASTC_10x10 = 0x93DB,
        SRGB8_Alpha8_ASTC_12x10 = 0x93DC,
        SRGB8_Alpha8_ASTC_12x12 = 0x93DD,

        SRGB8 = 0x8C41,
        SRGB8_Alpha8 = 0x8C43,
        SRGB_DXT1 = 0x8C4C,
        SRGB_Alpha_DXT1 = 0x8C4D,
        SRGB_Alpha_DXT3 = 0x8C4E,
        SRGB_Alpha_DXT5 = 0x8C4F,
        SRGB_BP_UNorm = 0x8E8D,
    };
    Q_ENUM_NS(TextureFormat)

    enum TextureWrapMode {
        Repeat = 0x2901,
        MirroredRepeat = 0x8370,
        ClampToEdge = 0x812F,
        ClampToBorder = 0x812D,
    };
    Q_ENUM_NS(TextureWrapMode)

    enum TextureFilter {
        Nearest = 0x2600,
        Linear = 0x2601,
        NearestMipMapNearest = 0x2700,
        NearestMipMapLinear = 0x2702,
        LinearMipMapNearest = 0x2701,
        LinearMipMapLinear = 0x2703,
    };
    Q_ENUM_NS(TextureFilter)

    enum DataType {
        Int8 = 0x1400, // GL_BYTE
        Int16 = 0x1402, // GL_SHORT
        Int32 = 0x1404, // GL_INT
        Int64 = 0x140E, // GL_INT64_ARB
        Uint8 = 0x1401, // GL_UNSIGNED_BYTE
        Uint16 = 0x1403, // GL_UNSIGNED_SHORT
        Uint32 = 0x1405, // GL_UNSIGNED_INT
        Uint64 = 0x140F, // GL_UNSIGNED_INT64_ARB
        Float = 0x1406, // GL_FLOAT
        Double = 0x140A, // GL_DOUBLE
    };
    Q_ENUM_NS(DataType)

    enum ShaderType {
        Includable = 0,

        Vertex = 0x8B31, // GL_VERTEX_SHADER
        Fragment = 0x8B30, // GL_FRAGMENT_SHADER
        Geometry = 0x8DD9, // GL_GEOMETRY_SHADER
        TessControl = 0x8E88, // GL_TESS_CONTROL_SHADER
        TessEvaluation = 0x8E87, // GL_TESS_EVALUATION_SHADER
        Compute = 0x91B9, // GL_COMPUTE_SHADER
        Task = 0x955A, // GL_TASK_SHADER_NV
        Mesh = 0x9559, // GL_MESH_SHADER_NV

        RayGeneration = 0x10000,
        RayIntersection,
        RayAnyHit,
        RayClosestHit,
        RayMiss,
        RayCallable,
    };
    Q_ENUM_NS(ShaderType)

    enum ShaderLanguage {
        None,
        GLSL,
        HLSL,
        Slang,
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
        NoComparisonFunc = 0, // GL_NONE
        LessEqual = 0x0203, // GL_LEQUAL
        GreaterEqual = 0x0206, // GL_GEQUAL
        Less = 0x0201, // GL_LESS
        Greater = 0x0204, // GL_GREATER
        Equal = 0x0202, // GL_EQUAL
        NotEqual = 0x0205, // GL_NOTEQUAL
        Always = 0x0207, // GL_ALWAYS
        Never = 0x0200, // GL_NEVER
    };
    Q_ENUM_NS(ComparisonFunc)

    enum BindingEditor {
        Expression,
        Expression2,
        Expression3,
        Expression4,
        Expression2x2,
        Expression2x3,
        Expression2x4,
        Expression3x2,
        Expression3x3,
        Expression3x4,
        Expression4x2,
        Expression4x3,
        Expression4x4,
        Color
    };
    Q_ENUM_NS(BindingEditor)

    enum ImageBindingFormat {
        Internal = 0, // GL_NONE

        // 8 bit
        r8 = 0x8229, // GL_R8
        r8ui = 0x8232, // GL_R8UI
        r8i = 0x8231, // GL_R8I

        // 16 bit
        r16 = 0x822A, // GL_R16
        r16_snorm = 0x8F98, // GL_R16_SNORM, not supported by glTexBuffer
        r16f = 0x822D, // GL_R16F
        r16ui = 0x8234, // GL_R16UI
        r16i = 0x8233, // GL_R16I
        rg8 = 0x822B, // GL_RG8
        rg8_snorm = 0x8F95, // GL_RG8_SNORM, not supported by glTexBuffer
        rg8ui = 0x8238, // GL_RG8UI
        rg8i = 0x8237, // GL_RG8I

        // 24 bit - not supported by glBindImageTexture
        rgb32f = 0x8815, // GL_RGB32F
        rgb32i = 0x8D83, // GL_RGB32I
        rgb32ui = 0x8D71, // GL_RGB32UI

        // 32 bit
        r32f = 0x822E, // GL_R32F
        r32ui = 0x8236, // GL_R32UI
        r32i = 0x8235, // GL_R32I
        rg16 = 0x822C, // GL_RG16
        rg16_snorm = 0x8F99, // GL_RG16_SNORM, not supported by glTexBuffer
        rg16ui = 0x823A, // GL_RG16UI
        rg16i = 0x8239, // GL_RG16I
        rg16f = 0x822F, // GL_RG16F
        rgba8 = 0x8058, // GL_RGBA8
        rgba8_snorm = 0x8F97, // GL_RGBA8_SNORM, not supported by glTexBuffer
        rgba8ui = 0x8D7C, // GL_RGBA8UI
        rgba8i = 0x8D8E, // GL_RGBA8I
        rgb10_a2 = 0x8059, // GL_RGB10_A2, not supported by glTexBuffer
        rgb10_a2ui = 0x906F, // GL_RGB10_A2UI, not supported by glTexBuffer
        r11f_g11f_b10f = 0x8C3A, // GL_R11F_G11F_B10F, not supported by glTexBuffer

        // 64 bit
        rg32f = 0x8230, // GL_RG32F
        rg32ui = 0x823C, // GL_RG32UI
        rg32i = 0x823B, // GL_RG32I
        rgba16 = 0x805B, // GL_RGBA16
        rgba16_snorm = 0x8F9B, // GL_RGBA16_SNORM, not supported by glTexBuffer
        rgba16f = 0x881A, // GL_RGBA16F
        rgba16ui = 0x8D76, // GL_RGBA16UI
        rgba16i = 0x8D88, // GL_RGBA16I

        // 128 bit
        rgba32f = 0x8814, // GL_RGBA32F
        rgba32i = 0x8D82, // GL_RGBA32I
        rgba32ui = 0x8D70, // GL_RGBA32UI
    };
    Q_ENUM_NS(ImageBindingFormat)

    enum FrontFace {
        CCW = 0x0901, // GL_CCW
        CW = 0x0900, // GL_CW
    };
    Q_ENUM_NS(FrontFace)

    enum CullMode {
        NoCulling = 0, // GL_NONE
        Back = 0x0405, // GL_BACK
        Front = 0x0404, // GL_FRONT
        FrontAndBack = 0x0408 // GL_FRONT_AND_BACK
    };
    Q_ENUM_NS(CullMode)

    enum PolygonMode {
        Fill = 0x1B02, // GL_FILL
        Line = 0x1B01, // GL_LINE
        Point = 0x1B00, // GL_POINT
    };
    Q_ENUM_NS(PolygonMode)

    enum LogicOperation {
        NoLogicOperation = 0, // GL_NONE
        Copy = 0x1503, // GL_COPY
        Clear = 0x1500, // GL_CLEAR
        Set = 0x150F, // GL_SET
        CopyInverted = 0x150C, // GL_COPY_INVERTED
        NoOp = 0x1505, // GL_NOOP
        Invert = 0x150A, // GL_INVERT
        And = 0x1501, // GL_AND
        Nand = 0x150E, // GL_NAND
        Or = 0x1507, // GL_OR
        Nor = 0x1508, // GL_NOR
        Xor = 0x1506, // GL_XOR
        Equiv = 0x1509, // GL_EQUIV
        AndReverse = 0x1502, // GL_AND_REVERSE
        AndInverted = 0x1504, // GL_AND_INVERTED
        OrReverse = 0x150B, // GL_OR_REVERSE
        OrInverted = 0x150D, // GL_OR_INVERTED
    };
    Q_ENUM_NS(LogicOperation)

    enum BlendEquation {
        Add = 0x8006, // GL_FUNC_ADD
        Min = 0x8007, // GL_MIN
        Max = 0x8008, // GL_MAX
        Subtract = 0x800A, // GL_FUNC_SUBTRACT
        ReverseSubtract = 0x800B, // GL_FUNC_REVERSE_SUBTRACT
    };
    Q_ENUM_NS(BlendEquation)

    enum BlendFactor {
        Zero = 0, // GL_ZERO
        One = 1, // GL_ONE
        SrcColor = 0x0300, // GL_SRC_COLOR
        OneMinusSrcColor = 0x0301, // GL_ONE_MINUS_SRC_COLOR
        SrcAlpha = 0x0302, // GL_SRC_ALPHA
        OneMinusSrcAlpha = 0x0303, // GL_ONE_MINUS_SRC_ALPHA
        DstAlpha = 0x0304, // GL_DST_ALPHA
        OneMinusDstAlpha = 0x0305, // GL_ONE_MINUS_DST_ALPHA
        DstColor = 0x0306, // GL_DST_COLOR
        OneMinusDstColor = 0x0307, // GL_ONE_MINUS_DST_COLOR
        SrcAlphaSaturate = 0x0308, // GL_SRC_ALPHA_SATURATE
        ConstantColor = 0x8001, // GL_CONSTANT_COLOR
        OneMinusConstantColor = 0x8002, // GL_ONE_MINUS_CONSTANT_COLOR
        ConstantAlpha = 0x8003, // GL_CONSTANT_ALPHA
        OneMinusConstantAlpha = 0x8004, // GL_ONE_MINUS_CONSTANT_ALPHA
    };
    Q_ENUM_NS(BlendFactor)

    enum PrimitiveType {
        Points = 0x0000, // GL_POINTS
        LineStrip = 0x0003, // GL_LINE_STRIP
        //LineLoop = 0x0002, // GL_LINE_LOOP
        Lines = 0x0001, // GL_LINES
        LineStripAdjacency = 0x000B, // GL_LINE_STRIP_ADJACENCY
        LinesAdjacency = 0x000A, // GL_LINES_ADJACENCY
        TriangleStrip = 0x0005, // GL_TRIANGLE_STRIP
        TriangleFan = 0x0006, // GL_TRIANGLE_FAN
        Triangles = 0x0004, // GL_TRIANGLES
        TriangleStripAdjacency = 0x000D, // GL_TRIANGLE_STRIP_ADJACENCY
        TrianglesAdjacency = 0x000C, // GL_TRIANGLES_ADJACENCY
        Patches = 0x000E // GL_PATCHES
    };
    Q_ENUM_NS(PrimitiveType)

    enum ExecuteOn {
        ResetEvaluation,
        ManualEvaluation,
        EveryEvaluation,
    };
    Q_ENUM_NS(ExecuteOn)

    enum Renderer {
        OpenGL,
        Vulkan,
        Direct3D,
    };
    Q_ENUM_NS(Renderer)
} // namespace ItemEnums

// enums moved to another namespace because of name clashes
namespace ItemEnums2 {
    Q_NAMESPACE

    enum CallType {
        Draw, // glDrawArrays(InstancedBaseInstance)
        DrawIndexed, // glDrawElements(InstancedBaseVertexBaseInstance)
        DrawIndirect, // gl(Multi)DrawArraysIndirect)
        DrawIndexedIndirect, // gl(Multi)DrawElementsIndirect
        DrawMeshTasks,
        DrawMeshTasksIndirect,
        Compute,
        ComputeIndirect,
        TraceRays,
        ClearTexture,
        CopyTexture,
        SwapTextures,
        ClearBuffer,
        CopyBuffer,
        SwapBuffers,

    };
    Q_ENUM_NS(CallType)

    enum StencilOperation {
        Keep = 0x1E00, // GL_KEEP
        Zero = 0, // GL_ZERO
        Replace = 0x1E01, // GL_REPLACE
        Increment = 0x1E02, // GL_INCR
        IncrementWrap = 0x8507, // GL_INCR_WRAP
        Decrement = 0x1E03, // GL_DECR
        DecrementWrap = 0x8508, // GL_DECR_WRAP
        Invert = 0x150A, // GL_INVERT
    };
    Q_ENUM_NS(StencilOperation)

    enum GeometryType {
        AxisAlignedBoundingBoxes,
        Triangles,
    };
    Q_ENUM_NS(GeometryType)

    enum ShaderCompiler {
        Driver,
        glslang,
        D3DCompiler,
        DXC,
        Slang,
    };
    Q_ENUM_NS(ShaderCompiler)

    enum ShaderCompilerSetting : int {
        autoMapBindings,
        autoMapLocations,
        autoSampledTextures,
        vulkanRulesRelaxed,
        spirvVersion,
        COUNT,
    };
    Q_ENUM_NS(ShaderCompilerSetting)
} // namespace ItemEnums2
