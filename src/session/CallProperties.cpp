#include "CallProperties.h"
#include "SessionModel.h"
#include "PropertiesEditor.h"
#include "ui_CallProperties.h"
#include <QDataWidgetMapper>

CallProperties::CallProperties(PropertiesEditor *propertiesEditor)
    : QWidget(propertiesEditor)
    , mPropertiesEditor(*propertiesEditor)
    , mUi(new Ui::CallProperties)
{
    mUi->setupUi(this);

    fillComboBox<Call::CallType>(mUi->type);
    fillComboBox<Call::PrimitiveType>(mUi->primitiveType);
    fillComboBox<Call::ExecuteOn>(mUi->executeOn);

    connect(mUi->type, &DataComboBox::currentDataChanged, this,
        &CallProperties::updateWidgets);
    connect(mUi->texture, &ReferenceComboBox::currentDataChanged, this,
        &CallProperties::updateWidgets);
    connect(mUi->primitiveType, &DataComboBox::currentDataChanged, this,
        &CallProperties::updateWidgets);

    for (auto combobox : { mUi->program, mUi->vertexStream, mUi->target,
             mUi->indexBufferBlock, mUi->indirectBufferBlock, mUi->texture,
             mUi->fromTexture, mUi->buffer, mUi->fromBuffer })
        connect(combobox, &ReferenceComboBox::textRequired,
            [this](QVariant data) {
                return mPropertiesEditor.getItemName(data.toInt());
            });

    connect(mUi->program, &ReferenceComboBox::listRequired,
        [this]() { return mPropertiesEditor.getItemIds(Item::Type::Program); });
    connect(mUi->vertexStream, &ReferenceComboBox::listRequired, [this]() {
        return mPropertiesEditor.getItemIds(Item::Type::Stream, true);
    });
    connect(mUi->target, &ReferenceComboBox::listRequired,
        [this]() { return mPropertiesEditor.getItemIds(Item::Type::Target); });
    for (auto block : { mUi->indexBufferBlock, mUi->indirectBufferBlock })
        connect(block, &ReferenceComboBox::listRequired, [this]() {
            return mPropertiesEditor.getItemIds(Item::Type::Block);
        });
    for (auto buffer : { mUi->buffer, mUi->fromBuffer })
        connect(buffer, &ReferenceComboBox::listRequired, [this]() {
            return mPropertiesEditor.getItemIds(Item::Type::Buffer);
        });
    for (auto texture : { mUi->texture, mUi->fromTexture })
        connect(texture, &ReferenceComboBox::listRequired, [this]() {
            return mPropertiesEditor.getItemIds(Item::Type::Texture);
        });

    updateWidgets();
}

CallProperties::~CallProperties()
{
    delete mUi;
}

Call::CallType CallProperties::currentType() const
{
    return static_cast<Call::CallType>(mUi->type->currentData().toInt());
}

Call::PrimitiveType CallProperties::currentPrimitiveType() const
{
    return static_cast<Call::PrimitiveType>(
        mUi->primitiveType->currentData().toInt());
}

CallKind CallProperties::currentCallKind() const
{
    if (QObject::sender() == mUi->type)
        mPropertiesEditor.updateModel();

    if (auto call = mPropertiesEditor.model().item<Call>(
            mPropertiesEditor.currentModelIndex()))
        return getKind(*call);
    return {};
}

TextureKind CallProperties::currentTextureKind() const
{
    if (auto texture = castItem<Texture>(mPropertiesEditor.model().findItem(
            mUi->texture->currentData().toInt())))
        return getKind(*texture);
    return {};
}

void CallProperties::addMappings(QDataWidgetMapper &mapper)
{
    mapper.addMapping(mUi->name, SessionModel::Name);
    mapper.addMapping(mUi->type, SessionModel::CallType);
    mapper.addMapping(mUi->executeOn, SessionModel::CallExecuteOn);

    mapper.addMapping(mUi->program, SessionModel::CallProgramId);
    mapper.addMapping(mUi->target, SessionModel::CallTargetId);
    mapper.addMapping(mUi->vertexStream, SessionModel::CallVertexStreamId);

    mapper.addMapping(mUi->primitiveType, SessionModel::CallPrimitiveType);
    mapper.addMapping(mUi->patchVertices, SessionModel::CallPatchVertices);
    mapper.addMapping(mUi->vertexCount, SessionModel::CallCount);
    mapper.addMapping(mUi->firstVertex, SessionModel::CallFirst);

    mapper.addMapping(mUi->indexBufferBlock,
        SessionModel::CallIndexBufferBlockId);
    mapper.addMapping(mUi->baseVertex, SessionModel::CallBaseVertex);

    mapper.addMapping(mUi->instanceCount, SessionModel::CallInstanceCount);
    mapper.addMapping(mUi->baseInstance, SessionModel::CallBaseInstance);

    mapper.addMapping(mUi->indirectBufferBlock,
        SessionModel::CallIndirectBufferBlockId);
    mapper.addMapping(mUi->drawCount, SessionModel::CallDrawCount);

    mapper.addMapping(mUi->workGroupsX, SessionModel::CallWorkGroupsX);
    mapper.addMapping(mUi->workGroupsY, SessionModel::CallWorkGroupsY);
    mapper.addMapping(mUi->workGroupsZ, SessionModel::CallWorkGroupsZ);

    mapper.addMapping(mUi->texture, SessionModel::CallTextureId);
    mapper.addMapping(mUi->fromTexture, SessionModel::CallFromTextureId);
    mapper.addMapping(mUi->clearColor, SessionModel::CallClearColor);
    mapper.addMapping(mUi->clearDepth, SessionModel::CallClearDepth);
    mapper.addMapping(mUi->clearStencil, SessionModel::CallClearStencil);

    mapper.addMapping(mUi->buffer, SessionModel::CallBufferId);
    mapper.addMapping(mUi->fromBuffer, SessionModel::CallFromBufferId);
}

void CallProperties::updateWidgets()
{
    const auto type = currentType();
    const auto kind = currentCallKind();
    setFormVisibility(mUi->formLayout, mUi->labelProgram, mUi->program,
        kind.draw || kind.compute);
    setFormVisibility(mUi->formLayout, mUi->labelTarget, mUi->target,
        kind.draw);
    setFormVisibility(mUi->formLayout, mUi->labelVertexStream,
        mUi->vertexStream, kind.draw);
    setFormVisibility(mUi->formLayout, mUi->labelIndexBufferBlock,
        mUi->indexBufferBlock, kind.indexed);
    setFormVisibility(mUi->formLayout, mUi->labelPrimitiveType,
        mUi->primitiveType, kind.draw);
    setFormVisibility(mUi->formLayout, mUi->labelIndirectBufferBlock,
        mUi->indirectBufferBlock, kind.indirect);

    setFormVisibility(mUi->formLayout, mUi->labelPatchVertices,
        mUi->patchVertices,
        currentPrimitiveType() == Call::PrimitiveType::Patches);
    setFormVisibility(mUi->formLayout, mUi->labelVertexCount, mUi->vertexCount,
        kind.draw && !kind.indirect);
    setFormVisibility(mUi->formLayout, mUi->labelInstanceCount,
        mUi->instanceCount, kind.draw && !kind.indirect);
    setFormVisibility(mUi->formLayout, mUi->labelFirstVertex, mUi->firstVertex,
        kind.draw && !kind.indirect);
    setFormVisibility(mUi->formLayout, mUi->labelBaseVertex, mUi->baseVertex,
        kind.draw && kind.indexed && !kind.indirect);
    setFormVisibility(mUi->formLayout, mUi->labelBaseInstance,
        mUi->baseInstance, kind.draw && !kind.indirect);
    setFormVisibility(mUi->formLayout, mUi->labelDrawCount, mUi->drawCount,
        kind.draw && kind.indirect);

    setFormVisibility(mUi->formLayout, mUi->labelWorkGroupsX, mUi->workGroupsX,
        kind.compute && !kind.indirect);
    setFormVisibility(mUi->formLayout, mUi->labelWorkGroupsY, mUi->workGroupsY,
        kind.compute && !kind.indirect);
    setFormVisibility(mUi->formLayout, mUi->labelWorkGroupsZ, mUi->workGroupsZ,
        kind.compute && !kind.indirect);

    setFormVisibility(mUi->formLayout, mUi->labelFromTexture, mUi->fromTexture,
        type == Call::CallType::CopyTexture
            || type == Call::CallType::SwapTextures);

    setFormVisibility(mUi->formLayout, mUi->labelTexture, mUi->texture,
        type == Call::CallType::ClearTexture
            || type == Call::CallType::CopyTexture
            || type == Call::CallType::SwapTextures);

    const auto texKind = currentTextureKind();
    setFormVisibility(mUi->formLayout, mUi->labelClearColor, mUi->clearColor,
        type == Call::CallType::ClearTexture && texKind.color);
    setFormVisibility(mUi->formLayout, mUi->labelClearDepth, mUi->clearDepth,
        type == Call::CallType::ClearTexture && texKind.depth);
    setFormVisibility(mUi->formLayout, mUi->labelClearStencil,
        mUi->clearStencil,
        type == Call::CallType::ClearTexture && texKind.stencil);

    setFormVisibility(mUi->formLayout, mUi->labelBuffer, mUi->buffer,
        type == Call::CallType::ClearBuffer
            || type == Call::CallType::CopyBuffer
            || type == Call::CallType::SwapBuffers);

    setFormVisibility(mUi->formLayout, mUi->labelFromBuffer, mUi->fromBuffer,
        type == Call::CallType::CopyBuffer
            || type == Call::CallType::SwapBuffers);
}
