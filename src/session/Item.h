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
    Primitives,
    Attribute,
    Framebuffer,
    Attachment,
    Call,
    State,
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
    bool checked{ true };
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

    int size() const {
        switch (dataType) {
            case Column::Int8: return 1;
            case Column::Int16: return 2;
            case Column::Int32: return 4;
            //case Column::Int64: return 8;
            case Column::Uint8: return 1;
            case Column::Uint16: return 2;
            case Column::Uint32: return 4;
            //case Column::Uint64: return 8;
            case Column::Float: return 4;
            case Column::Double: return 8;
        }
        return 0;
    }
};

struct Buffer : FileItem
{
    int offset{ 0 };
    int rowCount{ 1 };

    int stride() const {
        auto stride = 0;
        foreach (const Item* item, items) {
            auto& column = *static_cast<const Column*>(item);
            stride += column.count * column.size() + column.padding;
        }
        return stride;
    }

    int columnOffset(const Column* col) const {
        auto offset = this->offset;
        foreach (const Item* item, items) {
            auto& column = *static_cast<const Column*>(item);
            if (&column == col)
                return offset;
            offset += column.count * column.size() + column.padding;
        }
        return offset;
    }
};

struct Texture : FileItem
{
    using Target = QOpenGLTexture::Target;
    using Format = QOpenGLTexture::TextureFormat;

    Target target{ QOpenGLTexture::Target2D };
    Format format{ QOpenGLTexture::RGBA8_UNorm };
    int width{ 256 };
    int height{ 256 };
    int depth{ 1 };
    bool flipY{ };
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

    Type type{ Fragment };
};

struct Binding : Item
{
    enum Type {
        Uniform,
        Texture,
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

    QStringList getValue(int index) const {
        return (index < 0 || index >= values.size() ?
                QStringList() : values[index].toStringList());
    }

    QString getField(int valueIndex, int fieldIndex) const {
        auto value = getValue(valueIndex);
        return (fieldIndex < 0 || fieldIndex >= value.size() ?
                QString() : value[fieldIndex]);
    }
};

struct Primitives : Item
{
    enum Type {
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

    ItemId indexBufferId{ };
    Type type{ Triangles };
    int firstVertex{ 0 };
    int vertexCount{ 3 };
    int instanceCount{ 1 };
    int patchVertices{ 1 };
    int primitiveRestartIndex{ 0 };
};

struct Attribute : Item
{
    ItemId bufferId{ };
    ItemId columnId{ };
    bool normalize{ };
    int divisor{ };
};

struct Framebuffer : Item
{
};

struct Attachment : Item
{
    ItemId textureId{ };
};

struct Call : Item
{
    enum Type {
        Draw,
        Compute,
    };

    Type type{ };
    ItemId programId{ };
    ItemId primitivesId{ };
    ItemId framebufferId{ };
    int numGroupsX{ 1 };
    int numGroupsY{ 1 };
    int numGroupsZ{ 1 };
};

struct State : Item
{
    enum Type {
    };

    Type type{ };
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
template<> inline ItemType getItemType<Primitives>() { return ItemType::Primitives; }
template<> inline ItemType getItemType<Attribute>() { return ItemType::Attribute; }
template<> inline ItemType getItemType<Framebuffer>() { return ItemType::Framebuffer; }
template<> inline ItemType getItemType<Attachment>() { return ItemType::Attachment; }
template<> inline ItemType getItemType<Call>() { return ItemType::Call; }
template<> inline ItemType getItemType<State>() { return ItemType::State; }
template<> inline ItemType getItemType<Script>() { return ItemType::Script; }

template <typename T>
const T* castItem(const Item* item)
{
    if (item && item->itemType == getItemType<T>())
        return static_cast<const T*>(item);
    return nullptr;
}

#endif // ITEM_H
