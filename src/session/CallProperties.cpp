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
        { "Draw Indirect", Call::DrawIndirect },
        { "Compute", Call::Compute },
        //{ "Clear Texture", Call::ClearTexture },
        //{ "Clear Buffer", Call::ClearBuffer },
        //{ "Generate Mipmaps", Call::GenerateMipmaps },
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

    for (auto combobox : { mUi->program, mUi->vertexStream, mUi->framebuffer,
            mUi->indexBuffer, mUi->indirectBuffer, mUi->texture, mUi->buffer })
        connect(combobox, &ReferenceComboBox::textRequired,
            [this](QVariant data) { return mSessionProperties.findItemName(data.toInt()); });

    connect(mUi->program, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(ItemType::Program); });
    connect(mUi->vertexStream, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(ItemType::VertexStream); });
    connect(mUi->framebuffer, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(ItemType::Framebuffer, true); });
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

void CallProperties::addMappings(QDataWidgetMapper &mapper)
{
    mapper.addMapping(mUi->type, SessionModel::CallType);

    mapper.addMapping(mUi->program, SessionModel::CallProgramId);
    mapper.addMapping(mUi->framebuffer, SessionModel::CallFramebufferId);
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
    mapper.addMapping(mUi->buffer, SessionModel::CallBufferId);
    mapper.addMapping(mUi->clearColor, SessionModel::CallValues);
}

void CallProperties::updateWidgets()
{
    const auto type = currentType();
    const auto draw = (type == Call::Draw);
    const auto drawIndirect = (type == Call::DrawIndirect);
    const auto anyDraw = (draw || drawIndirect);
    const auto compute = (type == Call::Compute);
    const auto clearTexture = (type == Call::ClearTexture);
    const auto clearBuffer = (type == Call::ClearBuffer);
    const auto genMipmaps = (type == Call::GenerateMipmaps);

    setFormVisibility(mUi->formLayout, mUi->labelProgram, mUi->program, anyDraw || compute);
    setFormVisibility(mUi->formLayout, mUi->labelFramebuffer, mUi->framebuffer, anyDraw);
    setFormVisibility(mUi->formLayout, mUi->labelVertexStream, mUi->vertexStream, anyDraw);
    setFormVisibility(mUi->formLayout, mUi->labelIndexBuffer, mUi->indexBuffer, anyDraw);

    setFormVisibility(mUi->formLayout, mUi->labelPrimitiveType, mUi->primitiveType, anyDraw);
    setFormVisibility(mUi->formLayout, mUi->labelVertexCount, mUi->vertexCount, draw);
    setFormVisibility(mUi->formLayout, mUi->labelInstanceCount, mUi->instanceCount, draw);
    setFormVisibility(mUi->formLayout, mUi->labelFirstVertex, mUi->firstVertex, draw);
    setFormVisibility(mUi->formLayout, mUi->labelBaseVertex, mUi->baseVertex, draw);
    setFormVisibility(mUi->formLayout, mUi->labelBaseInstance, mUi->baseInstance, draw);

    setFormVisibility(mUi->formLayout, mUi->labelIndirectBuffer, mUi->indirectBuffer, drawIndirect);
    setFormVisibility(mUi->formLayout, mUi->labelDrawCount, mUi->drawCount, drawIndirect);

    setFormVisibility(mUi->formLayout, mUi->labelWorkGroupsX, mUi->workGroupsX, compute);
    setFormVisibility(mUi->formLayout, mUi->labelWorkGroupsY, mUi->workGroupsY, compute);
    setFormVisibility(mUi->formLayout, mUi->labelWorkGroupsZ, mUi->workGroupsZ, compute);

    setFormVisibility(mUi->formLayout, mUi->labelTexture, mUi->texture, clearTexture || genMipmaps);
    setFormVisibility(mUi->formLayout, mUi->labelBuffer, mUi->buffer, clearBuffer);
    setFormVisibility(mUi->formLayout, mUi->labelClearColor, mUi->clearColor, clearTexture);
    setFormVisibility(mUi->formLayout, mUi->labelClearValue, mUi->clearValue, clearBuffer);
}
