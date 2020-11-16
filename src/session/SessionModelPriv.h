#ifndef SESSIONMODELPRIV_H
#define SESSIONMODELPRIV_H

#include "Item.h"
#include <QMetaEnum>
#include <QJsonObject>
#include <QJsonArray>

#define ADD_EACH_COLUMN_TYPE() \
    ADD(GroupInlineScope, Group, inlineScope) \
    ADD(BufferOffset, Buffer, offset) \
    ADD(BufferRowCount, Buffer, rowCount) \
    ADD(ColumnDataType, Column, dataType) \
    ADD(ColumnCount, Column, count) \
    ADD(ColumnPadding, Column, padding) \
    ADD(TextureTarget, Texture, target) \
    ADD(TextureFormat, Texture, format) \
    ADD(TextureWidth, Texture, width) \
    ADD(TextureHeight, Texture, height) \
    ADD(TextureDepth, Texture, depth) \
    ADD(TextureLayers, Texture, layers) \
    ADD(TextureSamples, Texture, samples) \
    ADD(ScriptExecuteOn, Script, executeOn) \
    ADD(ScriptExpression, Script, expression) \
    ADD(ShaderType, Shader, shaderType) \
    ADD(BindingType, Binding, bindingType) \
    ADD(BindingEditor, Binding, editor) \
    ADD(BindingValues, Binding, values) \
    ADD(BindingTextureId, Binding, textureId) \
    ADD(BindingBufferId, Binding, bufferId) \
    ADD(BindingLevel, Binding, level) \
    ADD(BindingLayer, Binding, layer) \
    ADD(BindingImageFormat, Binding, imageFormat) \
    ADD(BindingMinFilter, Binding, minFilter) \
    ADD(BindingMagFilter, Binding, magFilter) \
    ADD(BindingAnisotropic, Binding, anisotropic) \
    ADD(BindingWrapModeX, Binding, wrapModeX) \
    ADD(BindingWrapModeY, Binding, wrapModeY) \
    ADD(BindingWrapModeZ, Binding, wrapModeZ) \
    ADD(BindingBorderColor, Binding, borderColor) \
    ADD(BindingComparisonFunc, Binding, comparisonFunc) \
    ADD(BindingSubroutine, Binding, subroutine) \
    ADD(AttributeBufferId, Attribute, bufferId) \
    ADD(AttributeColumnId, Attribute, columnId) \
    ADD(AttributeNormalize, Attribute, normalize) \
    ADD(AttributeDivisor, Attribute, divisor) \
    ADD(AttachmentTextureId, Attachment, textureId) \
    ADD(AttachmentLevel, Attachment, level) \
    ADD(AttachmentLayer, Attachment, layer) \
    ADD(AttachmentBlendColorEq, Attachment, blendColorEq) \
    ADD(AttachmentBlendColorSource, Attachment, blendColorSource) \
    ADD(AttachmentBlendColorDest, Attachment, blendColorDest) \
    ADD(AttachmentBlendAlphaEq, Attachment, blendAlphaEq) \
    ADD(AttachmentBlendAlphaSource, Attachment, blendAlphaSource) \
    ADD(AttachmentBlendAlphaDest, Attachment, blendAlphaDest) \
    ADD(AttachmentColorWriteMask, Attachment, colorWriteMask) \
    ADD(AttachmentDepthComparisonFunc, Attachment, depthComparisonFunc) \
    ADD(AttachmentDepthOffsetFactor, Attachment, depthOffsetFactor) \
    ADD(AttachmentDepthOffsetUnits, Attachment, depthOffsetUnits) \
    ADD(AttachmentDepthClamp, Attachment, depthClamp) \
    ADD(AttachmentDepthWrite, Attachment, depthWrite) \
    ADD(AttachmentStencilFrontComparisonFunc, Attachment, stencilFrontComparisonFunc) \
    ADD(AttachmentStencilFrontReference, Attachment, stencilFrontReference) \
    ADD(AttachmentStencilFrontReadMask, Attachment, stencilFrontReadMask) \
    ADD(AttachmentStencilFrontFailOp, Attachment, stencilFrontFailOp) \
    ADD(AttachmentStencilFrontDepthFailOp, Attachment, stencilFrontDepthFailOp) \
    ADD(AttachmentStencilFrontDepthPassOp, Attachment, stencilFrontDepthPassOp) \
    ADD(AttachmentStencilFrontWriteMask, Attachment, stencilFrontWriteMask) \
    ADD(AttachmentStencilBackComparisonFunc, Attachment, stencilBackComparisonFunc) \
    ADD(AttachmentStencilBackReference, Attachment, stencilBackReference) \
    ADD(AttachmentStencilBackReadMask, Attachment, stencilBackReadMask) \
    ADD(AttachmentStencilBackFailOp, Attachment, stencilBackFailOp) \
    ADD(AttachmentStencilBackDepthFailOp, Attachment, stencilBackDepthFailOp) \
    ADD(AttachmentStencilBackDepthPassOp, Attachment, stencilBackDepthPassOp) \
    ADD(AttachmentStencilBackWriteMask, Attachment, stencilBackWriteMask) \
    ADD(TargetFrontFace, Target, frontFace) \
    ADD(TargetCullMode, Target, cullMode) \
    ADD(TargetLogicOperation, Target, logicOperation) \
    ADD(TargetBlendConstant, Target, blendConstant) \
    ADD(TargetDefaultWidth, Target, defaultWidth) \
    ADD(TargetDefaultHeight, Target, defaultHeight) \
    ADD(TargetDefaultLayers, Target, defaultLayers) \
    ADD(TargetDefaultSamples, Target, defaultSamples) \
    ADD(CallChecked, Call, checked) \
    ADD(CallType, Call, callType) \
    ADD(CallProgramId, Call, programId) \
    ADD(CallTargetId, Call, targetId) \
    ADD(CallVertexStreamId, Call, vertexStreamId) \
    ADD(CallPrimitiveType, Call, primitiveType) \
    ADD(CallPatchVertices, Call, patchVertices) \
    ADD(CallCount, Call, count) \
    ADD(CallFirst, Call, first) \
    ADD(CallIndexBufferId, Call, indexBufferId) \
    ADD(CallBaseVertex, Call, baseVertex) \
    ADD(CallInstanceCount, Call, instanceCount) \
    ADD(CallBaseInstance, Call, baseInstance) \
    ADD(CallIndirectBufferId, Call, indirectBufferId) \
    ADD(CallIndirectOffset, Call, indirectOffset) \
    ADD(CallDrawCount, Call, drawCount) \
    ADD(CallWorkGroupsX, Call, workGroupsX) \
    ADD(CallWorkGroupsY, Call, workGroupsY) \
    ADD(CallWorkGroupsZ, Call, workGroupsZ) \
    ADD(CallBufferId, Call, bufferId) \
    ADD(CallFromBufferId, Call, fromBufferId) \
    ADD(CallTextureId, Call, textureId) \
    ADD(CallFromTextureId, Call, fromTextureId) \
    ADD(CallClearColor, Call, clearColor) \
    ADD(CallClearDepth, Call, clearDepth) \
    ADD(CallClearStencil, Call, clearStencil) \
    ADD(CallExecuteOn, Call, executeOn) \

template<typename T>
auto fromVariant(const QVariant &v)
     -> std::enable_if_t<std::is_enum<T>::value, T>
{
    // check if it is a valid key string
    auto ok = false;
    auto value = QMetaEnum::fromType<T>().keyToValue(
        qPrintable(v.toString()), &ok);
    if (ok)
        return static_cast<T>(value);
    // check if it is already a valid value
    if (QMetaEnum::fromType<T>().valueToKey(v.toInt()))
        return static_cast<T>(v.toInt());
    return T{ };
}

template<typename T>
auto fromVariant(const QVariant &v)
     -> std::enable_if_t<!std::is_enum<T>::value, T>;

template<>
inline int fromVariant<int>(const QVariant &v)
{
    return v.toInt();
}

template<>
inline unsigned int fromVariant<unsigned int>(const QVariant &v)
{
    return v.toUInt();
}

template<>
inline double fromVariant<double>(const QVariant &v)
{
    return v.toDouble();
}

template<>
inline bool fromVariant<bool>(const QVariant &v)
{
    return v.toBool();
}

template<>
inline QString fromVariant<QString>(const QVariant &v)
{
    return v.toString();
}

template<>
inline QStringList fromVariant<QStringList>(const QVariant &v)
{
    return v.toStringList();
}

template<>
inline QColor fromVariant<QColor>(const QVariant &v)
{
    return QColor(v.toString());
}

template<typename F>
void forEachItem(const Item &item, const F &function)
{
    function(item);
    for (const auto *child : qAsConst(item.items))
        forEachItem(*child, function);
}

template<typename T>
auto toJsonValue(const T &v)
     -> std::enable_if_t<!std::is_enum<T>::value, QJsonValue>
{
    return v;
}

template<typename T>
auto toJsonValue(const T &v)
     -> std::enable_if_t<std::is_enum<T>::value, QJsonValue>
{
    return QMetaEnum::fromType<T>().valueToKey(v);
}

template<>
inline QJsonValue toJsonValue(const QStringList &v)
{
    auto array = QJsonArray();
    for (const auto &value : v)
        array.append(value);
    return array;
}

template<>
inline QJsonValue toJsonValue(const unsigned int &v)
{
    return static_cast<int>(v);
}

template<>
inline QJsonValue toJsonValue(const QColor &v)
{
    return v.name(QColor::HexArgb);
}

#endif // SESSIONMODELPRIV_H
