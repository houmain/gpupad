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
    connect(mUi->primitiveType, &DataComboBox::currentDataChanged,
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

Call::PrimitiveType CallProperties::currentPrimitiveType() const
{
    return static_cast<Call::PrimitiveType>(
        mUi->primitiveType->currentData().toInt());
}

CallKind CallProperties::currentCallKind() const
{
    if (auto texture = mSessionProperties.model().item<Call>(
            mSessionProperties.currentModelIndex()))
        return getKind(*texture);
    return { };
}

TextureKind CallProperties::currentTextureKind() const
{
    if (auto texture = castItem<Texture>(mSessionProperties.model().findItem(
            mUi->texture->currentData().toInt())))
        return getKind(*texture);
    return { };
}

void CallProperties::addMappings(QDataWidgetMapper &mapper)
{
    mapper.addMapping(mUi->type, SessionModel::CallType);

    mapper.addMapping(mUi->program, SessionModel::CallProgramId);
    mapper.addMapping(mUi->target, SessionModel::CallTargetId);
    mapper.addMapping(mUi->vertexStream, SessionModel::CallVertexStreamId);

    mapper.addMapping(mUi->primitiveType, SessionModel::CallPrimitiveType);
    mapper.addMapping(mUi->patchVertices, SessionModel::CallPatchVertices);
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
    const auto kind = currentCallKind();
    setFormVisibility(mUi->formLayout, mUi->labelProgram, mUi->program,
        kind.draw || kind.compute);
    setFormVisibility(mUi->formLayout, mUi->labelTarget, mUi->target,
        kind.draw);
    setFormVisibility(mUi->formLayout, mUi->labelVertexStream, mUi->vertexStream,
        kind.draw);
    setFormVisibility(mUi->formLayout, mUi->labelIndexBuffer, mUi->indexBuffer,
        kind.drawIndexed);
    setFormVisibility(mUi->formLayout, mUi->labelIndirectBuffer, mUi->indirectBuffer,
        kind.drawIndirect);

    setFormVisibility(mUi->formLayout, mUi->labelPrimitiveType, mUi->primitiveType,
        kind.draw);
    setFormVisibility(mUi->formLayout, mUi->labelPatchVertices, mUi->patchVertices,
        currentPrimitiveType() == Call::Patches);
    setFormVisibility(mUi->formLayout, mUi->labelVertexCount, mUi->vertexCount,
        kind.drawDirect);
    setFormVisibility(mUi->formLayout, mUi->labelInstanceCount, mUi->instanceCount,
        kind.drawDirect);
    setFormVisibility(mUi->formLayout, mUi->labelFirstVertex, mUi->firstVertex,
        kind.drawDirect);
    setFormVisibility(mUi->formLayout, mUi->labelBaseVertex, mUi->baseVertex,
        kind.drawIndexed && kind.drawDirect);
    setFormVisibility(mUi->formLayout, mUi->labelBaseInstance, mUi->baseInstance,
        kind.drawDirect);
    setFormVisibility(mUi->formLayout, mUi->labelDrawCount, mUi->drawCount,
        kind.drawIndirect);

    setFormVisibility(mUi->formLayout, mUi->labelWorkGroupsX, mUi->workGroupsX,
        kind.compute);
    setFormVisibility(mUi->formLayout, mUi->labelWorkGroupsY, mUi->workGroupsY,
        kind.compute);
    setFormVisibility(mUi->formLayout, mUi->labelWorkGroupsZ, mUi->workGroupsZ,
        kind.compute);

    setFormVisibility(mUi->formLayout, mUi->labelTexture, mUi->texture,
        type == Call::ClearTexture || type == Call::GenerateMipmaps);

    auto texKind = currentTextureKind();
    setFormVisibility(mUi->formLayout, mUi->labelClearColor, mUi->clearColor,
        type == Call::ClearTexture && texKind.color);
    setFormVisibility(mUi->formLayout, mUi->labelClearDepth, mUi->clearDepth,
        type == Call::ClearTexture && texKind.depth);
    setFormVisibility(mUi->formLayout, mUi->labelClearStencil, mUi->clearStencil,
        type == Call::ClearTexture && texKind.stencil);

    setFormVisibility(mUi->formLayout, mUi->labelBuffer, mUi->buffer,
        type == Call::ClearBuffer);
}
