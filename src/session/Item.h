#pragma once

#include "ItemEnums.h"
#include "Evaluation.h"
#include <array>
#include <QList>
#include <QVariant>
#include <QOpenGLTexture>

using ItemId = int; 
struct Item;

struct Item
{
    using Type = ItemEnums::ItemType;

    virtual ~Item() = default;

    ItemId id{ };
    Type type{ };
    Item *parent{ };
    QList<Item*> items;
    QString name;
};

struct FileItem : Item
{
    QString fileName;
};

struct Group : Item
{
    bool inlineScope{ };
    QString iterations{ "1" };
};

struct Buffer : FileItem
{
};

struct Block : Item 
{
    QString offset{ "0" };
    QString rowCount{ "1" };
};

struct Field : Item
{
    using DataType = ItemEnums::DataType;

    DataType dataType{ DataType::Float };
    int count{ 1 };
    int padding{ 0 };
};

struct Texture : FileItem
{
    using Target = QOpenGLTexture::Target;
    using Format = QOpenGLTexture::TextureFormat;

    Target target{ QOpenGLTexture::Target2D };
    Format format{ QOpenGLTexture::RGBA8_UNorm };
    QString width{ "256" };
    QString height{ "256" };
    QString depth{ "1" };
    QString layers{ "1" };
    int samples{ 1 };
    bool flipVertically{ };
};

struct Program : Item
{
};

struct Shader : FileItem
{
    using ShaderType = ItemEnums::ShaderType;
    using Language = ItemEnums::ShaderLanguage;

    ShaderType shaderType{ ShaderType::Vertex };
    Language language{ Language::GLSL };
    QString entryPoint;
    QString preamble;
    QString includePaths;
};

struct Binding : Item
{
    using BindingType = ItemEnums::BindingType;
    using Filter = QOpenGLTexture::Filter;
    using WrapMode = QOpenGLTexture::WrapMode;
    using ComparisonFunc = ItemEnums::ComparisonFunc;
    using Editor = ItemEnums::BindingEditor;
    using ImageFormat = ItemEnums::ImageBindingFormat;

    BindingType bindingType{ BindingType::Uniform };
    Editor editor{ Editor::Expression };
    QStringList values{ "0" };
    ItemId textureId{ };
    ItemId bufferId{ };
    ItemId blockId{ };
    int level{ };
    int layer{ -1 };
    Filter minFilter{ QOpenGLTexture::Nearest };
    Filter magFilter{ QOpenGLTexture::Nearest };
    bool anisotropic{ };
    WrapMode wrapModeX{ QOpenGLTexture::Repeat };
    WrapMode wrapModeY{ QOpenGLTexture::Repeat };
    WrapMode wrapModeZ{ QOpenGLTexture::Repeat };
    QColor borderColor{ Qt::black };
    ComparisonFunc comparisonFunc{ ComparisonFunc::NoComparisonFunc };
    QString subroutine;
    ImageFormat imageFormat{ ImageFormat::Internal };
};

struct Stream : Item
{
};

struct Attribute : Item
{
    ItemId fieldId{ };
    bool normalize{ };
    int divisor{ };
};

struct Target : Item
{
    using FrontFace = ItemEnums::FrontFace;
    using CullMode = ItemEnums::CullMode;
    using PolygonMode = ItemEnums::PolygonMode;
    using LogicOperation = ItemEnums::LogicOperation;

    FrontFace frontFace{ FrontFace::CCW };
    CullMode cullMode{ CullMode::NoCulling };
    PolygonMode polygonMode{ PolygonMode::Fill };
    LogicOperation logicOperation{ LogicOperation::NoLogicOperation };
    QColor blendConstant{ Qt::white };
    int defaultWidth{ 1 };
    int defaultHeight{ 1 };
    int defaultLayers{ 0 };
    int defaultSamples{ 0 };
};

struct Attachment : Item
{
    using BlendEquation = ItemEnums::BlendEquation;
    using BlendFactor = ItemEnums::BlendFactor;
    using ComparisonFunc = ItemEnums::ComparisonFunc;
    using StencilOperation = ItemEnums2::StencilOperation;

    ItemId textureId{ };
    int level{ };
    int layer{ -1 };

    BlendEquation blendColorEq{ BlendEquation::Add };
    BlendFactor blendColorSource{ BlendFactor::One };
    BlendFactor blendColorDest{ BlendFactor::Zero };
    BlendEquation blendAlphaEq{ BlendEquation::Add };
    BlendFactor blendAlphaSource{ BlendFactor::One };
    BlendFactor blendAlphaDest{ BlendFactor::Zero };
    unsigned int colorWriteMask{ 0xF };

    ComparisonFunc depthComparisonFunc{ ComparisonFunc::Less };
    double depthOffsetSlope{ };
    double depthOffsetConstant{ };
    bool depthClamp{ };
    bool depthWrite{ true };

    ComparisonFunc stencilFrontComparisonFunc{ ComparisonFunc::Always };
    int stencilFrontReference{ };
    unsigned int stencilFrontReadMask{ 0xFF };
    StencilOperation stencilFrontFailOp{ StencilOperation::Keep };
    StencilOperation stencilFrontDepthFailOp{ StencilOperation::Keep };
    StencilOperation stencilFrontDepthPassOp{ StencilOperation::Keep };
    unsigned int stencilFrontWriteMask{ 0xFF };

    ComparisonFunc stencilBackComparisonFunc{ ComparisonFunc::Always };
    int stencilBackReference{ };
    unsigned int stencilBackReadMask{ 0xFF };
    StencilOperation stencilBackFailOp{ StencilOperation::Keep };
    StencilOperation stencilBackDepthFailOp{ StencilOperation::Keep };
    StencilOperation stencilBackDepthPassOp{ StencilOperation::Keep };
    unsigned int stencilBackWriteMask{ 0xFF };
};

struct Call : Item
{
    using CallType = ItemEnums2::CallType;
    using PrimitiveType = ItemEnums::PrimitiveType;
    using ExecuteOn = ItemEnums::ExecuteOn;

    ExecuteOn executeOn{ ExecuteOn::EveryEvaluation };
    bool checked{ true };
    CallType callType{ };
    ItemId programId{ };
    ItemId targetId{ };
    ItemId vertexStreamId{ };

    PrimitiveType primitiveType{ PrimitiveType::Triangles };
    QString count{ "3" };
    QString first{ "0" };

    ItemId indexBufferBlockId{ };
    QString baseVertex{ "0" };

    QString instanceCount{ "1" };
    QString baseInstance{ "0" };

    ItemId indirectBufferBlockId{ };
    QString drawCount{ "1" };

    QString patchVertices{ "3" };

    QString workGroupsX{ "1" };
    QString workGroupsY{ "1" };
    QString workGroupsZ{ "1" };

    ItemId textureId{ };
    ItemId fromTextureId{ };
    QColor clearColor{ Qt::black };
    double clearDepth{ 1.0 };
    int clearStencil{ };

    ItemId bufferId{ };
    ItemId fromBufferId{ };
};

struct Script : FileItem
{
    using ExecuteOn = ItemEnums::ExecuteOn;

    ExecuteOn executeOn{ ExecuteOn::ManualEvaluation };
};

struct TextureKind
{
    int dimensions;
    bool color;
    bool depth;
    bool stencil;
    bool array;
    bool cubeMap;
};

struct CallKind
{
    bool draw;
    bool indexed;
    bool indirect;
    bool patches;
    bool compute;
};

int getFieldSize(const Field &field);
int getFieldRowOffset(const Field &field);
int getBlockStride(const Block &block);
TextureKind getKind(const Texture &texture);
CallKind getKind(const Call &call);
bool shouldExecute(Call::ExecuteOn executeOn, EvaluationType evaluationType);

template<typename T> Item::Type getItemType();
template<> inline Item::Type getItemType<Group>() { return Item::Type::Group; }
template<> inline Item::Type getItemType<Buffer>() { return Item::Type::Buffer; }
template<> inline Item::Type getItemType<Block>() { return Item::Type::Block; }
template<> inline Item::Type getItemType<Field>() { return Item::Type::Field; }
template<> inline Item::Type getItemType<Texture>() { return Item::Type::Texture; }
template<> inline Item::Type getItemType<Program>() { return Item::Type::Program; }
template<> inline Item::Type getItemType<Shader>() { return Item::Type::Shader; }
template<> inline Item::Type getItemType<Binding>() { return Item::Type::Binding; }
template<> inline Item::Type getItemType<Stream>() { return Item::Type::Stream; }
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

