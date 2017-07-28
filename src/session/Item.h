#ifndef ITEM_H
#define ITEM_H

#include <QList>
#include <QVariant>
#include <QOpenGLTexture>

using ItemId = int;
struct Item;

enum class ItemType
{
    Group,
    Buffer,
    Column,
    Texture,
    Image,
    Sampler,
    Program,
    Shader,
    Binding,
    VertexStream,
    Attribute,
    Target,
    Attachment,
    Call,
    Script,
};

struct Item
{
    ItemId id{ };
    ItemType itemType{ };
    Item *parent{ };
    QList<Item*> items;
    QString name{ };
    bool inlineScope{ };
};

struct FileItem : Item
{
    QString fileName;
};

struct Group : Item
{
};

struct Column : Item
{
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

    DataType dataType{ DataType::Float };
    int count{ 1 };
    int padding{ 0 };

    int size() const
    {
        switch (dataType) {
            case Int8: return 1;
            case Int16: return 2;
            case Int32: return 4;
            //case Int64: return 8;
            case Uint8: return 1;
            case Uint16: return 2;
            case Uint32: return 4;
            //case Uint64: return 8;
            case Float: return 4;
            case Double: return 8;
        }
        return 0;
    }
};

struct Buffer : FileItem
{
    int offset{ 0 };
    int rowCount{ 1 };

    int stride() const
    {
        auto stride = 0;
        foreach (const Item* item, items) {
            auto& column = *static_cast<const Column*>(item);
            stride += column.count * column.size() + column.padding;
        }
        return stride;
    }

    int columnOffset(const Column* col) const
    {
        auto offset = 0;
        foreach (const Item* item, items) {
            const auto& column = *static_cast<const Column*>(item);
            if (&column == col)
                return offset;
            offset += column.count * column.size() + column.padding;
        }
        return offset;
    }
};

struct Texture : FileItem
{
    enum class Type {
        None,
        Color,
        Depth,
        DepthStencil,
        Stencil,
    };
    using Target = QOpenGLTexture::Target;
    using Format = QOpenGLTexture::TextureFormat;

    Target target{ QOpenGLTexture::Target2D };
    Format format{ QOpenGLTexture::RGBA8_UNorm };
    int width{ 256 };
    int height{ 256 };
    int depth{ 1 };
    int samples{ 1 };
    bool flipY{ };

    Type type() const
    {
        switch (format) {
            case QOpenGLTexture::D16: return Type::Depth;
            case QOpenGLTexture::D24: return Type::Depth;
            case QOpenGLTexture::D32: return Type::Depth;
            case QOpenGLTexture::D32F: return Type::Depth;
            case QOpenGLTexture::D24S8: return Type::DepthStencil;
            case QOpenGLTexture::D32FS8X24: return Type::DepthStencil;
            case QOpenGLTexture::S8: return Type::Stencil;
            default: return Type::Color;
        }
    }
};

struct Image : FileItem
{
    using Face = QOpenGLTexture::CubeMapFace;

    int level{ };
    int layer{ };
    Face face{ };
};

struct Sampler : Item
{
    using Filter = QOpenGLTexture::Filter;
    using WrapMode = QOpenGLTexture::WrapMode;

    ItemId textureId{ };
    Filter minFilter{ QOpenGLTexture::Nearest };
    Filter magFilter{ QOpenGLTexture::Nearest };
    WrapMode wrapModeX{ QOpenGLTexture::Repeat };
    WrapMode wrapModeY{ QOpenGLTexture::Repeat };
    WrapMode wrapModeZ{ QOpenGLTexture::Repeat };
};

struct Program : Item
{
};

struct Shader : FileItem
{
    enum Type {
        Header = 0,
        Vertex = GL_VERTEX_SHADER,
        Fragment = GL_FRAGMENT_SHADER,
        Geometry = GL_GEOMETRY_SHADER,
        TessellationControl = GL_TESS_CONTROL_SHADER,
        TessellationEvaluation = GL_TESS_EVALUATION_SHADER,
        Compute = GL_COMPUTE_SHADER,
    };

    Type type{ Vertex };
};

struct Binding : Item
{
    enum Type {
        Uniform,
        Sampler,
        Image,
        Buffer,
        //AtomicCounterBuffer,
        //Subroutine,
    };

    enum Editor {
        Expression, Expression2, Expression3, Expression4,
        Expression2x2, Expression2x3, Expression2x4,
        Expression3x2, Expression3x3, Expression3x4,
        Expression4x2, Expression4x3, Expression4x4,
        Color
    };

    Type type{ };
    Editor editor{ };

    // each value is a QStringList (with up to 16 fields)
    QVariantList values;

    int valueCount() const { return values.size(); }

    QStringList getValue(int index) const
    {
        return (index < 0 || index >= values.size() ?
                QStringList() : values[index].toStringList());
    }

    QString getField(int valueIndex, int fieldIndex) const
    {
        auto value = getValue(valueIndex);
        return (fieldIndex < 0 || fieldIndex >= value.size() ?
                QString() : value[fieldIndex]);
    }
};

struct VertexStream : Item
{
};

struct Attribute : Item
{
    ItemId bufferId{ };
    ItemId columnId{ };
    bool normalize{ };
    int divisor{ };
};

struct Target : Item
{
    enum FrontFace {
        CCW = GL_CCW,
        CW = GL_CW,
    };

    enum CullMode {
        CullDisabled = GL_NONE,
        Back = GL_BACK,
        Front = GL_FRONT,
        FrontAndBack = GL_FRONT_AND_BACK
    };

    enum LogicOperation {
        LogicOperationDisabled = GL_NONE,
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

    FrontFace frontFace{ FrontFace::CCW };
    CullMode cullMode{ CullMode::CullDisabled };
    LogicOperation logicOperation{ LogicOperation::LogicOperationDisabled };
    QColor blendConstant{ Qt::white };
};

struct Attachment : Item
{
    enum BlendEquation {
        Add = GL_FUNC_ADD,
        Min = GL_MIN,
        Max = GL_MAX,
        Subtract = GL_FUNC_SUBTRACT,
        ReverseSubtract = GL_FUNC_REVERSE_SUBTRACT,
    };

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

    enum ComparisonFunction {
        Always = GL_ALWAYS,
        Never = GL_NEVER,
        Less = GL_LESS,
        Equal = GL_EQUAL,
        LessEqual = GL_LEQUAL,
        Greater = GL_GREATER,
        NotEqual = GL_NOTEQUAL,
        GreaterEqual = GL_GEQUAL,
    };

    enum StencilOperation {
        Keep = GL_KEEP,
        Reset = GL_ZERO,
        Replace = GL_REPLACE,
        Increment = GL_INCR,
        IncrementWrap = GL_INCR_WRAP,
        Decrement = GL_DECR,
        DecrementWrap = GL_DECR_WRAP,
        Invert = GL_INVERT,
    };

    ItemId textureId{ };
    int level{ };

    BlendEquation blendColorEq{ BlendEquation::Add };
    BlendFactor blendColorSource{ BlendFactor::One };
    BlendFactor blendColorDest{ BlendFactor::Zero };
    BlendEquation blendAlphaEq{ BlendEquation::Add };
    BlendFactor blendAlphaSource{ BlendFactor::One };
    BlendFactor blendAlphaDest{ BlendFactor::Zero };
    unsigned int colorWriteMask{ 0xF };

    ComparisonFunction depthCompareFunc{ ComparisonFunction::Less };
    float depthNear{ };
    float depthFar{ 1.0f };
    float depthBiasSlope{ };
    float depthBiasConst{ };
    bool depthClamp{ };
    bool depthWrite{ true };

    ComparisonFunction stencilFrontCompareFunc{ ComparisonFunction::Always };
    unsigned int stencilFrontReference{ };
    unsigned int stencilFrontReadMask{ 0xFF };
    StencilOperation stencilFrontFailOp{ StencilOperation::Keep };
    StencilOperation stencilFrontDepthFailOp{ StencilOperation::Keep };
    StencilOperation stencilFrontDepthPassOp{ StencilOperation::Keep };
    unsigned int stencilFrontWriteMask{ 0xFF };

    ComparisonFunction stencilBackCompareFunc{ ComparisonFunction::Always };
    unsigned int stencilBackReference{ };
    unsigned int stencilBackReadMask{ 0xFF };
    StencilOperation stencilBackFailOp{ StencilOperation::Keep };
    StencilOperation stencilBackDepthFailOp{ StencilOperation::Keep };
    StencilOperation stencilBackDepthPassOp{ StencilOperation::Keep };
    unsigned int stencilBackWriteMask{ 0xFF };
};

struct Call : Item
{
    enum Type {
        Draw, // glDrawArrays(InstancedBaseInstance)
        DrawIndexed, // glDrawElements(InstancedBaseVertexBaseInstance)
        DrawIndirect, // gl(Multi)DrawArraysIndirect)
        DrawIndexedIndirect, // gl(Multi)DrawElementsIndirect
        Compute,
        ClearTexture,
        ClearBuffer,
        GenerateMipmaps,
    };

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

    bool checked{ true };
    Type type{ };
    ItemId programId{ };
    ItemId targetId{ };
    ItemId vertexStreamId{ };

    PrimitiveType primitiveType{ Triangles };
    int count{ 3 };
    int first{ 0 };

    ItemId indexBufferId{ };
    int baseVertex{ };

    int instanceCount{ 1 };
    int baseInstance{ 0 };

    ItemId indirectBufferId{ };
    int drawCount{ 1 };

    int workGroupsX{ 1 };
    int workGroupsY{ 1 };
    int workGroupsZ{ 1 };

    ItemId textureId{ };
    QColor clearColor{ Qt::black };
    float clearDepth{ 1.0 };
    int clearStencil{ };

    ItemId bufferId{ };
};

struct Script : FileItem
{
};

template<typename T> ItemType getItemType();
template<> inline ItemType getItemType<Group>() { return ItemType::Group; }
template<> inline ItemType getItemType<Buffer>() { return ItemType::Buffer; }
template<> inline ItemType getItemType<Column>() { return ItemType::Column; }
template<> inline ItemType getItemType<Texture>() { return ItemType::Texture; }
template<> inline ItemType getItemType<Image>() { return ItemType::Image; }
template<> inline ItemType getItemType<Sampler>() { return ItemType::Sampler; }
template<> inline ItemType getItemType<Program>() { return ItemType::Program; }
template<> inline ItemType getItemType<Shader>() { return ItemType::Shader; }
template<> inline ItemType getItemType<Binding>() { return ItemType::Binding; }
template<> inline ItemType getItemType<VertexStream>() { return ItemType::VertexStream; }
template<> inline ItemType getItemType<Attribute>() { return ItemType::Attribute; }
template<> inline ItemType getItemType<Target>() { return ItemType::Target; }
template<> inline ItemType getItemType<Attachment>() { return ItemType::Attachment; }
template<> inline ItemType getItemType<Call>() { return ItemType::Call; }
template<> inline ItemType getItemType<Script>() { return ItemType::Script; }

template <typename T>
const T* castItem(const Item &item)
{
    if (item.itemType == getItemType<T>())
        return static_cast<const T*>(&item);
    return nullptr;
}

template <typename T>
const T* castItem(const Item *item)
{
    return (item ? castItem<T>(*item) : nullptr);
}

#endif // ITEM_H
