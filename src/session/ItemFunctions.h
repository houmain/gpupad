#ifndef ITEMFUNCTIONS_H
#define ITEMFUNCTIONS_H

#include "Item.h"

inline int getSize(const Column &column)
{
    switch (column.dataType) {
        case Column::DataType::Int8: return 1;
        case Column::DataType::Int16: return 2;
        case Column::DataType::Int32: return 4;
        //case Column::DataType::Int64: return 8;
        case Column::DataType::Uint8: return 1;
        case Column::DataType::Uint16: return 2;
        case Column::DataType::Uint32: return 4;
        //case Column::DataType::Uint64: return 8;
        case Column::DataType::Float: return 4;
        case Column::DataType::Double: return 8;
    }
    return 0;
}

inline int getColumnOffset(const Column &column)
{
    auto offset = 0;
    const auto &buffer = *static_cast<const Buffer*>(column.parent);
    foreach (const Item* item, buffer.items) {
        const auto& col = *static_cast<const Column*>(item);
        if (&column == &col)
            return offset;
        offset += col.count * getSize(col) + col.padding;
    }
    return offset;
}

inline int getStride(const Buffer &buffer)
{
    auto stride = 0;
    foreach (const Item* item, buffer.items) {
        auto& column = *static_cast<const Column*>(item);
        stride += column.count * getSize(column) + column.padding;
    }
    return stride;
}

struct TextureKind
{
    int dimensions;
    bool color;
    bool depth;
    bool stencil;
    bool array;
    bool multisample;
    bool cubeMap;
};

inline TextureKind getKind(const Texture &texture)
{
    auto kind = TextureKind{ };

    switch (texture.target) {
        case QOpenGLTexture::Target1D:
        case QOpenGLTexture::Target1DArray:
            kind.dimensions = 1;
            break;
        case QOpenGLTexture::Target3D:
            kind.dimensions = 3;
            break;
        default:
            kind.dimensions = 2;
    }

    switch (texture.target) {
        case QOpenGLTexture::Target1DArray:
        case QOpenGLTexture::Target2DArray:
        case QOpenGLTexture::TargetCubeMapArray:
        case QOpenGLTexture::Target2DMultisampleArray:
            kind.array = true;
            break;
        default:
            break;
    }

    switch (texture.target) {
        case QOpenGLTexture::Target2DMultisample:
        case QOpenGLTexture::Target2DMultisampleArray:
            kind.multisample = true;
            break;
        default:
            break;
    }

    switch (texture.target) {
        case QOpenGLTexture::TargetCubeMap:
        case QOpenGLTexture::TargetCubeMapArray:
            kind.cubeMap = true;
            break;
        default:
            break;
    }

    switch (texture.format) {
        case QOpenGLTexture::D16:
        case QOpenGLTexture::D24:
        case QOpenGLTexture::D32:
        case QOpenGLTexture::D32F:
            kind.depth = true;
            break;
        case QOpenGLTexture::D24S8:
        case QOpenGLTexture::D32FS8X24:
            kind.depth = kind.stencil = true;
            break;
        case QOpenGLTexture::S8:
            kind.stencil = true;
            break;
        default:
            kind.color = true;
    }
    return kind;
}

struct CallKind
{
    bool draw;
    bool drawIndexed;
    bool drawIndirect;
    bool drawDirect;
    bool drawPatches;
    bool compute;
};

inline CallKind getKind(const Call &call)
{
    auto kind = CallKind{ };

    switch (call.callType) {
        case Call::CallType::Draw:
            kind.drawDirect = true;
            break;
        case Call::CallType::DrawIndexed:
            kind.drawDirect = kind.drawIndexed = true;
            break;
        case Call::CallType::DrawIndirect:
            kind.drawIndirect = true;
            break;
        case Call::CallType::DrawIndexedIndirect:
            kind.drawIndirect = kind.drawIndexed = true;
            break;
        case Call::CallType::Compute:
            kind.compute = true;
            break;
        default:
            break;
    }

    kind.draw = (kind.drawDirect || kind.drawIndirect);

    if (call.primitiveType == Call::PrimitiveType::Patches)
        kind.drawPatches = true;

    return kind;
}

template<typename T> Item::Type getItemType();
template<> inline Item::Type getItemType<Group>() { return Item::Type::Group; }
template<> inline Item::Type getItemType<Buffer>() { return Item::Type::Buffer; }
template<> inline Item::Type getItemType<Column>() { return Item::Type::Column; }
template<> inline Item::Type getItemType<Texture>() { return Item::Type::Texture; }
template<> inline Item::Type getItemType<Image>() { return Item::Type::Image; }
template<> inline Item::Type getItemType<Program>() { return Item::Type::Program; }
template<> inline Item::Type getItemType<Shader>() { return Item::Type::Shader; }
template<> inline Item::Type getItemType<Binding>() { return Item::Type::Binding; }
template<> inline Item::Type getItemType<VertexStream>() { return Item::Type::VertexStream; }
template<> inline Item::Type getItemType<Attribute>() { return Item::Type::Attribute; }
template<> inline Item::Type getItemType<Target>() { return Item::Type::Target; }
template<> inline Item::Type getItemType<Attachment>() { return Item::Type::Attachment; }
template<> inline Item::Type getItemType<Call>() { return Item::Type::Call; }
template<> inline Item::Type getItemType<Script>() { return Item::Type::Script; }

template <typename T>
const T* castItem(const Item &item)
{
    if (item.type == getItemType<T>())
        return static_cast<const T*>(&item);
    return nullptr;
}

template <>
inline const FileItem* castItem<FileItem>(const Item &item) {
    if (item.type == Item::Type::Buffer ||
        item.type == Item::Type::Texture ||
        item.type == Item::Type::Image ||
        item.type == Item::Type::Shader ||
        item.type == Item::Type::Script)
        return static_cast<const FileItem*>(&item);
    return nullptr;
}

template <typename T>
const T* castItem(const Item *item)
{
    return (item ? castItem<T>(*item) : nullptr);
}

#endif // ITEMFUNCTIONS_H
