#include "CallProperties.h"
#include "SessionModel.h"
#include "SessionProperties.h"
#include "ui_CallProperties.h"
#include <QDataWidgetMapper>

CallProperties::CallProperties(SessionProperties *sessionProperties)
    : QWidget(sessionProperties)
    , mSessionProperties(*sessionProperties)
    , mUi(new Ui::CallProperties)
{
    mUi->setupUi(this);

    fill<Call::Type>(mUi->type, {
        { "Draw", Call::Draw },
        { "Draw Indexed", Call::DrawIndexed },
        { "Draw Indirect", Call::DrawIndirect },
        { "Draw Indexed Indirect", Call::DrawIndexedIndirect },
        { "Compute", Call::Compute },
        { "Clear Texture", Call::ClearTexture },
        { "Clear Buffer", Call::ClearBuffer },
        { "Generate Mipmaps", Call::GenerateMipmaps },
    });

    fill<Call::PrimitiveType>(mUi->primitiveType, {
        { "Points", Call::Points },
        { "Lines", Call::Lines },
        { "Line Strip", Call::LineStrip },
        { "Line Loop", Call::LineLoop },
        { "Triangles", Call::Triangles },
        { "Triangle Strip", Call::TriangleStrip },
        { "Triangle Fan", Call::TriangleFan },
        { "Lines Adjacency", Call::LinesAdjacency },
        { "Line Strip Adjacency", Call::LineStripAdjacency },
        { "Triangle Strip Addjacency", Call::TriangleStripAdjacency },
        { "Triangles Adjacency", Call::TrianglesAdjacency },
        { "Patches", Call::Patches },
    });

    connect(mUi->type, &DataComboBox::currentDataChanged,
        this, &CallProperties::updateWidgets);
    connect(mUi->texture, &ReferenceComboBox::currentDataChanged,
        this, &CallProperties::updateWidgets);

    for (auto combobox : { mUi->program, mUi->vertexStream, mUi->target,
            mUi->indexBuffer, mUi->indirectBuffer, mUi->texture, mUi->buffer })
        connect(combobox, &ReferenceComboBox::textRequired,
            [this](QVariant data) { return mSessionProperties.findItemName(data.toInt()); });

    connect(mUi->program, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(ItemType::Program); });
    connect(mUi->vertexStream, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(ItemType::VertexStream, true); });
    connect(mUi->target, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(ItemType::Target, true); });
    connect(mUi->indexBuffer, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(ItemType::Buffer, true); });
    connect(mUi->indirectBuffer, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(ItemType::Buffer); });
    connect(mUi->buffer, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(ItemType::Buffer); });
    connect(mUi->texture, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(ItemType::Texture); });

    updateWidgets();
}

CallProperties::~CallProperties()
{
    delete mUi;
}

Call::Type CallProperties::currentType() const
{
    return static_cast<Call::Type>(mUi->type->currentData().toInt());
}

Texture::Type CallProperties::currentTextureType() const
{
    if (!mUi->texture->currentData().isNull())
        if (auto texture = castItem<Texture>(mSessionProperties.model().findItem(
                mUi->texture->currentData().toInt())))
            return texture->type();

    return Texture::Type::None;
}

void CallProperties::addMappings(QDataWidgetMapper &mapper)
{
    mapper.addMapping(mUi->type, SessionModel::CallType);

    mapper.addMapping(mUi->program, SessionModel::CallProgramId);
    mapper.addMapping(mUi->target, SessionModel::CallTargetId);
    mapper.addMapping(mUi->vertexStream, SessionModel::CallVertexStreamId);

    mapper.addMapping(mUi->primitiveType, SessionModel::CallPrimitiveType);
    mapper.addMapping(mUi->vertexCount, SessionModel::CallCount);
    mapper.addMapping(mUi->firstVertex, SessionModel::CallFirst);

    mapper.addMapping(mUi->indexBuffer, SessionModel::CallIndexBufferId);
    mapper.addMapping(mUi->baseVertex, SessionModel::CallBaseVertex);

    mapper.addMapping(mUi->instanceCount, SessionModel::CallInstanceCount);
    mapper.addMapping(mUi->baseInstance, SessionModel::CallBaseInstance);

    mapper.addMapping(mUi->indirectBuffer, SessionModel::CallIndirectBufferId);
    mapper.addMapping(mUi->drawCount, SessionModel::CallDrawCount);

    mapper.addMapping(mUi->workGroupsX, SessionModel::CallWorkGroupsX);
    mapper.addMapping(mUi->workGroupsY, SessionModel::CallWorkGroupsY);
    mapper.addMapping(mUi->workGroupsZ, SessionModel::CallWorkGroupsZ);

    mapper.addMapping(mUi->texture, SessionModel::CallTextureId);
    mapper.addMapping(mUi->clearColor, SessionModel::CallClearColor);
    mapper.addMapping(mUi->clearDepth, SessionModel::CallClearDepth);
    mapper.addMapping(mUi->clearStencil, SessionModel::CallClearStencil);

    mapper.addMapping(mUi->buffer, SessionModel::CallBufferId);
}

void CallProperties::updateWidgets()
{
    const auto type = currentType();
    const auto drawIndexed = (type == Call::DrawIndexed || type == Call::DrawIndexedIndirect);
    const auto drawIndirect = (type == Call::DrawIndirect || type == Call::DrawIndexedIndirect);
    const auto draw = (type == Call::Draw || drawIndexed || drawIndirect);
    const auto drawDirect = (draw && !drawIndirect);
    const auto compute = (type == Call::Compute);
    const auto clearTexture = (type == Call::ClearTexture);
    const auto clearBuffer = (type == Call::ClearBuffer);
    const auto genMipmaps = (type == Call::GenerateMipmaps);

    setFormVisibility(mUi->formLayout, mUi->labelProgram, mUi->program, draw || compute);
    setFormVisibility(mUi->formLayout, mUi->labelTarget, mUi->target, draw);
    setFormVisibility(mUi->formLayout, mUi->labelVertexStream, mUi->vertexStream, draw);
    setFormVisibility(mUi->formLayout, mUi->labelIndexBuffer, mUi->indexBuffer, drawIndexed);
    setFormVisibility(mUi->formLayout, mUi->labelIndirectBuffer, mUi->indirectBuffer, drawIndirect);

    setFormVisibility(mUi->formLayout, mUi->labelPrimitiveType, mUi->primitiveType, draw);
    setFormVisibility(mUi->formLayout, mUi->labelVertexCount, mUi->vertexCount, drawDirect);
    setFormVisibility(mUi->formLayout, mUi->labelInstanceCount, mUi->instanceCount, drawDirect);
    setFormVisibility(mUi->formLayout, mUi->labelFirstVertex, mUi->firstVertex, drawDirect);
    setFormVisibility(mUi->formLayout, mUi->labelBaseVertex, mUi->baseVertex, drawIndexed && drawDirect);
    setFormVisibility(mUi->formLayout, mUi->labelBaseInstance, mUi->baseInstance, drawDirect);
    setFormVisibility(mUi->formLayout, mUi->labelDrawCount, mUi->drawCount, drawIndirect);

    setFormVisibility(mUi->formLayout, mUi->labelWorkGroupsX, mUi->workGroupsX, compute);
    setFormVisibility(mUi->formLayout, mUi->labelWorkGroupsY, mUi->workGroupsY, compute);
    setFormVisibility(mUi->formLayout, mUi->labelWorkGroupsZ, mUi->workGroupsZ, compute);

    setFormVisibility(mUi->formLayout, mUi->labelTexture, mUi->texture, clearTexture || genMipmaps);
    const auto textureType = currentTextureType();
    const auto color = (textureType == Texture::Type::Color);
    const auto depth = (textureType == Texture::Type::Depth ||
                        textureType == Texture::Type::DepthStencil);
    const auto stencil = (textureType == Texture::Type::Stencil ||
                          textureType == Texture::Type::DepthStencil);
    setFormVisibility(mUi->formLayout, mUi->labelClearColor, mUi->clearColor, clearTexture && color);
    setFormVisibility(mUi->formLayout, mUi->labelClearDepth, mUi->clearDepth, clearTexture && depth);
    setFormVisibility(mUi->formLayout, mUi->labelClearStencil, mUi->clearStencil, clearTexture && stencil);

    setFormVisibility(mUi->formLayout, mUi->labelBuffer, mUi->buffer, clearBuffer);
}
