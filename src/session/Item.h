#ifndef ITEM_H
#define ITEM_H

#include "ItemEnums.h"
#include <array>
#include <QList>
#include <QVariant>
#include <QOpenGLTexture>

using ItemId = int;
struct Item;

using ItemType = ItemEnums::ItemType;

struct Item
{
    virtual ~Item() = default;

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
    using DataType = ItemEnums::ColumnDataType;

    DataType dataType{ DataType::Float };
    int count{ 1 };
    int padding{ 0 };
};

struct Buffer : FileItem
{
    int offset{ 0 };
    int rowCount{ 1 };
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
    int layers{ 1 };
    int samples{ 1 };
    bool flipY{ };
};

struct Image : FileItem
{
    using CubeMapFace = QOpenGLTexture::CubeMapFace;

    int level{ };
    int layer{ };
    CubeMapFace face{ };
};

struct Program : Item
{
};

struct Shader : FileItem
{
    using Type = ItemEnums::ShaderType;

    Type type{ Type::Vertex };
};

struct Binding : Item
{
    using Type = ItemEnums::BindingType;
    using Filter = QOpenGLTexture::Filter;
    using WrapMode = QOpenGLTexture::WrapMode;
    using ComparisonFunc = ItemEnums::ComparisonFunc;
    using Editor = ItemEnums::BindingEditor;

    struct Value {
        QStringList fields;
        ItemId textureId{ };
        ItemId bufferId{ };
        int level{ };
        bool layered{ };
        int layer{ };
        Filter minFilter{ QOpenGLTexture::Nearest };
        Filter magFilter{ QOpenGLTexture::Nearest };
        WrapMode wrapModeX{ QOpenGLTexture::Repeat };
        WrapMode wrapModeY{ QOpenGLTexture::Repeat };
        WrapMode wrapModeZ{ QOpenGLTexture::Repeat };
        QColor borderColor{ Qt::black };
        ComparisonFunc comparisonFunc{ };
        QString subroutine;
    };

    Type type{ };
    Editor editor{ };
    int currentValue{ };
    int valueCount{ 1 };
    std::array<Value, 8> values{ };
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
    using FrontFace = ItemEnums::FrontFace;
    using CullMode = ItemEnums::CullMode;
    using LogicOperation = ItemEnums::LogicOperation;

    FrontFace frontFace{ };
    CullMode cullMode{ };
    LogicOperation logicOperation{ };
    QColor blendConstant{ Qt::white };
};

struct Attachment : Item
{
    using BlendEquation = ItemEnums::BlendEquation;
    using BlendFactor = ItemEnums::BlendFactor;
    using ComparisonFunc = ItemEnums::ComparisonFunc;
    using StencilOperation = ItemEnums2::StencilOperation;

    ItemId textureId{ };
    int level{ };
    bool layered{ };
    int layer{ };

    BlendEquation blendColorEq{ BlendEquation::Add };
    BlendFactor blendColorSource{ BlendFactor::One };
    BlendFactor blendColorDest{ BlendFactor::Zero };
    BlendEquation blendAlphaEq{ BlendEquation::Add };
    BlendFactor blendAlphaSource{ BlendFactor::One };
    BlendFactor blendAlphaDest{ BlendFactor::Zero };
    unsigned int colorWriteMask{ 0xF };

    ComparisonFunc depthComparisonFunc{ ComparisonFunc::Less };
    float depthOffsetFactor{ };
    float depthOffsetUnits{ };
    bool depthClamp{ };
    bool depthWrite{ true };

    ComparisonFunc stencilFrontComparisonFunc{ ComparisonFunc::Always };
    unsigned int stencilFrontReference{ };
    unsigned int stencilFrontReadMask{ 0xFF };
    StencilOperation stencilFrontFailOp{ StencilOperation::Keep };
    StencilOperation stencilFrontDepthFailOp{ StencilOperation::Keep };
    StencilOperation stencilFrontDepthPassOp{ StencilOperation::Keep };
    unsigned int stencilFrontWriteMask{ 0xFF };

    ComparisonFunc stencilBackComparisonFunc{ ComparisonFunc::Always };
    unsigned int stencilBackReference{ };
    unsigned int stencilBackReadMask{ 0xFF };
    StencilOperation stencilBackFailOp{ StencilOperation::Keep };
    StencilOperation stencilBackDepthFailOp{ StencilOperation::Keep };
    StencilOperation stencilBackDepthPassOp{ StencilOperation::Keep };
    unsigned int stencilBackWriteMask{ 0xFF };
};

struct Call : Item
{
    using Type = ItemEnums2::CallType;
    using PrimitiveType = ItemEnums::PrimitiveType;

    bool checked{ true };
    Type type{ };
    ItemId programId{ };
    ItemId targetId{ };
    ItemId vertexStreamId{ };

    PrimitiveType primitiveType{ PrimitiveType::Triangles };
    int count{ 3 };
    int first{ 0 };

    ItemId indexBufferId{ };
    int baseVertex{ };

    int instanceCount{ 1 };
    int baseInstance{ 0 };

    ItemId indirectBufferId{ };
    int drawCount{ 1 };

    int patchVertices{ 3 };

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

#endif // ITEM_H
