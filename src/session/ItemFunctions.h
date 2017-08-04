#ifndef ITEMFUNCTIONS_H
#define ITEMFUNCTIONS_H

#include "Item.h"

inline int getSize(const Column &column)
{
    switch (column.dataType) {
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

    switch (call.type) {
        case Call::Draw:
            kind.drawDirect = true;
            break;
        case Call::DrawIndexed:
            kind.drawDirect = kind.drawIndexed = true;
            break;
        case Call::DrawIndirect:
            kind.drawIndirect = true;
            break;
        case Call::DrawIndexedIndirect:
            kind.drawIndirect = kind.drawIndexed = true;
            break;
        case Call::Compute:
            kind.compute = true;
            break;
        default:
            break;
    }

    kind.draw = (kind.drawDirect || kind.drawIndirect);

    if (call.primitiveType == Call::Patches)
        kind.drawPatches = true;

    return kind;
}

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

template <>
inline const FileItem* castItem<FileItem>(const Item &item) {
    if (item.itemType == ItemType::Buffer ||
        item.itemType == ItemType::Texture ||
        item.itemType == ItemType::Image ||
        item.itemType == ItemType::Shader ||
        item.itemType == ItemType::Script)
        return static_cast<const FileItem*>(&item);
    return nullptr;
}

template <typename T>
const T* castItem(const Item *item)
{
    return (item ? castItem<T>(*item) : nullptr);
}

#endif // ITEMFUNCTIONS_H
