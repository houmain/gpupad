#include "PropertiesEditor.h"
#include "AttachmentProperties.h"
#include "BindingProperties.h"
#include "CallProperties.h"
#include "SessionProperties.h"
#include "FileCache.h"
#include "SessionModel.h"
#include "Settings.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "TextureProperties.h"
#include "editors/EditorManager.h"
#include "editors/binary/BinaryEditor.h"
#include "editors/source/SourceEditor.h"
#include "editors/texture/TextureEditor.h"
#include "ui_AttributeProperties.h"
#include "ui_BlockProperties.h"
#include "ui_BufferProperties.h"
#include "ui_FieldProperties.h"
#include "ui_GroupProperties.h"
#include "ui_ProgramProperties.h"
#include "ui_ScriptProperties.h"
#include "ui_ShaderProperties.h"
#include "ui_StreamProperties.h"
#include "ui_TargetProperties.h"
#include "ui_AccelerationStructureProperties.h"
#include "ui_InstanceProperties.h"
#include "ui_GeometryProperties.h"
#include <QDataWidgetMapper>
#include <QStackedWidget>
#include <QTimer>

namespace {
    class StackedWidget final : public QStackedWidget
    {
    public:
        using QStackedWidget::QStackedWidget;

        QSize minimumSizeHint() const override
        {
            return currentWidget()->minimumSizeHint();
        }
    };

    template <typename T>
    void instantiate(std::unique_ptr<T> &ptr)
    {
        ptr.reset(new T());
    }
} // namespace

QString splitPascalCase(QString str)
{
    static const auto regex = QRegularExpression("([a-z])([A-Z])");
    return str.replace(regex, "\\1 \\2");
}

void setFormVisibility(QFormLayout *layout, QLabel *label, QWidget *widget,
    bool visible)
{
    if (label)
        layout->removeWidget(label);
    layout->removeWidget(widget);
    if (visible)
        layout->addRow(label, widget);
    if (label)
        label->setVisible(visible);
    widget->setVisible(visible);
}

void setFormEnabled(QLabel *label, QWidget *widget, bool enabled)
{
    label->setEnabled(enabled);
    widget->setEnabled(enabled);
}

PropertiesEditor::PropertiesEditor(QWidget *parent)
    : QScrollArea(parent)
    , mModel(Singletons::sessionModel())
    , mStack(new StackedWidget(this))
    , mMapper(new QDataWidgetMapper(this))
    , mSubmitTimer(new QTimer(this))
{
    setFrameShape(QFrame::NoFrame);
    setBackgroundRole(QPalette::ToolTipBase);

    mMapper->setModel(&mModel);
    connect(mSubmitTimer, &QTimer::timeout, mMapper,
        &QDataWidgetMapper::submit);
    mSubmitTimer->start(100);

    const auto add = [&](auto &ui) {
        auto widget = new QWidget(this);
        instantiate(ui);
        ui->setupUi(widget);
        mStack->addWidget(widget);
    };
    mRootProperties = new QWidget(this);
    mStack->addWidget(mRootProperties);
    mSessionProperties = new SessionProperties(this);
    mStack->addWidget(mSessionProperties);
    add(mGroupProperties);
    add(mBufferProperties);
    add(mBlockProperties);
    add(mFieldProperties);
    mTextureProperties = new TextureProperties(this);
    mStack->addWidget(mTextureProperties);
    add(mProgramProperties);
    add(mShaderProperties);
    mBindingProperties = new BindingProperties(this);
    mStack->addWidget(mBindingProperties);
    add(mStreamProperties);
    add(mAttributeProperties);
    add(mTargetProperties);
    mAttachmentProperties = new AttachmentProperties(this);
    mStack->addWidget(mAttachmentProperties);
    mCallProperties = new CallProperties(this);
    mStack->addWidget(mCallProperties);
    add(mScriptProperties);
    add(mAccelerationStructureProperties);
    add(mInstanceProperties);
    add(mGeometryProperties);

    setWidgetResizable(true);
    setWidget(mStack);

    connect(mShaderProperties->fileNew, &QToolButton::clicked,
        [this]() { saveCurrentItemFileAs(FileDialog::ShaderExtensions); });
    connect(mShaderProperties->fileBrowse, &QToolButton::clicked,
        [this]() { openCurrentItemFile(FileDialog::ShaderExtensions); });
    connect(mShaderProperties->file, &ReferenceComboBox::listRequired,
        [this]() { return getFileNames(Item::Type::Shader); });
    connect(mShaderProperties->file, &ReferenceComboBox::activated, this,
        &PropertiesEditor::deduceShaderType);

    connect(mBufferProperties->fileNew, &QToolButton::clicked,
        [this]() { saveCurrentItemFileAs(FileDialog::BinaryExtensions); });
    connect(mBufferProperties->fileBrowse, &QToolButton::clicked,
        [this]() { openCurrentItemFile(FileDialog::BinaryExtensions); });
    connect(mBufferProperties->file, &ReferenceComboBox::listRequired,
        [this]() { return getFileNames(Item::Type::Buffer, true); });
    connect(mBlockProperties->deduceOffset, &QToolButton::clicked, this,
        &PropertiesEditor::deduceBlockOffset);
    connect(mBlockProperties->deduceRowCount, &QToolButton::clicked, this,
        &PropertiesEditor::deduceBlockRowCount);

    connect(mScriptProperties->fileNew, &QToolButton::clicked,
        [this]() { saveCurrentItemFileAs(FileDialog::ScriptExtensions); });
    connect(mScriptProperties->fileBrowse, &QToolButton::clicked,
        [this]() { openCurrentItemFile(FileDialog::ScriptExtensions); });
    connect(mScriptProperties->file, &ReferenceComboBox::listRequired,
        [this]() { return getFileNames(Item::Type::Script, true); });

    connect(mAttributeProperties->field, &ReferenceComboBox::listRequired,
        [this]() { return getItemIds(Item::Type::Field); });

    connect(mGeometryProperties->type, &DataComboBox::currentDataChanged, this,
        &PropertiesEditor::updateGeometryWidgets);
    connect(mGeometryProperties->vertexBufferBlock,
        &ReferenceComboBox::listRequired,
        [this]() { return getItemIds(Item::Type::Block); });
    connect(mGeometryProperties->indexBufferBlock,
        &ReferenceComboBox::listRequired,
        [this]() { return getItemIds(Item::Type::Block, true); });

    for (auto comboBox : { mShaderProperties->file, mBufferProperties->file,
             mScriptProperties->file })
        connect(comboBox, &ReferenceComboBox::textRequired, [](auto data) {
            return FileDialog::getFileTitle(data.toString());
        });

    for (auto comboBox :
        { mAttributeProperties->field, mGeometryProperties->vertexBufferBlock,
            mGeometryProperties->indexBufferBlock })
        connect(comboBox, &ReferenceComboBox::textRequired,
            [this](QVariant data) { return getItemName(data.toInt()); });

    auto &settings = Singletons::settings();
    connect(&settings, &Settings::fontChanged,
        mShaderProperties->shaderPreamble, &QPlainTextEdit::setFont);
    mShaderProperties->shaderPreamble->setFont(settings.font());

    setCurrentModelIndex(mModel.sessionItemIndex());
    fillComboBoxes();
}

PropertiesEditor::~PropertiesEditor() = default;

void PropertiesEditor::fillComboBoxes()
{
    fillComboBox<Field::DataType>(mFieldProperties->type);
    fillComboBox<Target::FrontFace>(mTargetProperties->frontFace);
    fillComboBox<Target::CullMode>(mTargetProperties->cullMode);
    fillComboBox<Target::PolygonMode>(mTargetProperties->polygonMode);
    fillComboBox<Target::LogicOperation>(mTargetProperties->logicOperation);
    fillComboBox<Shader::Language>(mShaderProperties->language);
    removeComboBoxItem(mShaderProperties->language, "None");
    fillComboBox<Shader::ShaderType>(mShaderProperties->type);
    renameComboBoxItem(mShaderProperties->type, "Tess Control",
        "Tessellation Control");
    renameComboBoxItem(mShaderProperties->type, "Tess Evaluation",
        "Tessellation Evaluation");
    fillComboBox<Script::ExecuteOn>(mScriptProperties->executeOn);
    fillComboBox<Geometry::GeometryType>(mGeometryProperties->type);
}

QVariantList PropertiesEditor::getFileNames(Item::Type type, bool addNull) const
{
    auto result = QVariantList();
    if (addNull)
        result += "";

    const auto append = [&](const QStringList fileNames) {
        for (const auto &fileName : fileNames)
            if (!result.contains(fileName))
                result.append(fileName);
    };
    switch (type) {
    case Item::Type::Shader:
    case Item::Type::Script:
        append(Singletons::editorManager().getSourceFileNames());
        break;

    case Item::Type::Buffer:
        append(Singletons::editorManager().getBinaryFileNames());
        break;

    case Item::Type::Texture:
        append(Singletons::editorManager().getImageFileNames());
        break;

    default: break;
    }

    mModel.forEachItem([&](const Item &item) {
        if (item.type == type) {
            const auto &fileName = static_cast<const FileItem &>(item).fileName;
            if (!result.contains(fileName))
                result.append(fileName);
        }
    });
    return result;
}

QString PropertiesEditor::getItemName(ItemId itemId) const
{
    return mModel.getFullItemName(itemId);
}

QVariantList PropertiesEditor::getItemIds(Item::Type type, bool addNull) const
{
    auto result = QVariantList();
    if (addNull)
        result += 0;

    mModel.forEachItemScoped(currentModelIndex(), [&](const Item &item) {
        if (item.type == type)
            result.append(item.id);
    });
    return result;
}

void PropertiesEditor::updateModel()
{
    mMapper->submit();
}

QModelIndex PropertiesEditor::currentModelIndex(int column) const
{
    return mModel.index(mMapper->currentIndex(), column, mMapper->rootIndex());
}

void PropertiesEditor::setCurrentModelIndex(const QModelIndex &index)
{
    mMapper->submit();
    mMapper->clearMapping();

    if (!index.isValid())
        return mStack->setCurrentIndex(0);

    const auto map = [&](QWidget *control, SessionModel::ColumnType column) {
        mMapper->addMapping(control, column);
    };

    switch (mModel.getItemType(index)) {
    case Item::Type::Root: break;

    case Item::Type::Session: mSessionProperties->addMappings(*mMapper); break;

    case Item::Type::Group:
        map(mGroupProperties->name, SessionModel::Name);
        map(mGroupProperties->inlineScope, SessionModel::GroupInlineScope);
        map(mGroupProperties->iterations, SessionModel::GroupIterations);
        break;

    case Item::Type::Buffer:
        map(mBufferProperties->file, SessionModel::FileName);
        break;

    case Item::Type::Block:
        map(mBlockProperties->name, SessionModel::Name);
        map(mBlockProperties->offset, SessionModel::BlockOffset);
        map(mBlockProperties->rowCount, SessionModel::BlockRowCount);
        updateBlockWidgets(index);
        break;

    case Item::Type::Field:
        map(mFieldProperties->name, SessionModel::Name);
        map(mFieldProperties->type, SessionModel::FieldDataType);
        map(mFieldProperties->count, SessionModel::FieldCount);
        map(mFieldProperties->padding, SessionModel::FieldPadding);
        break;

    case Item::Type::Texture: mTextureProperties->addMappings(*mMapper); break;

    case Item::Type::Program:
        map(mProgramProperties->name, SessionModel::Name);
        break;

    case Item::Type::Shader:
        map(mShaderProperties->file, SessionModel::FileName);
        map(mShaderProperties->language, SessionModel::ShaderLanguage);
        map(mShaderProperties->type, SessionModel::ShaderType);
        map(mShaderProperties->entryPoint, SessionModel::ShaderEntryPoint);
        map(mShaderProperties->shaderPreamble, SessionModel::ShaderPreamble);
        map(mShaderProperties->shaderIncludePaths,
            SessionModel::ShaderIncludePaths);
        break;

    case Item::Type::Binding: mBindingProperties->addMappings(*mMapper); break;

    case Item::Type::Stream:
        map(mStreamProperties->name, SessionModel::Name);
        break;

    case Item::Type::Attribute:
        map(mAttributeProperties->name, SessionModel::Name);
        map(mAttributeProperties->field, SessionModel::AttributeFieldId);
        map(mAttributeProperties->normalize, SessionModel::AttributeNormalize);
        map(mAttributeProperties->divisor, SessionModel::AttributeDivisor);
        break;

    case Item::Type::Target:
        map(mTargetProperties->name, SessionModel::Name);
        map(mTargetProperties->width, SessionModel::TargetDefaultWidth);
        map(mTargetProperties->height, SessionModel::TargetDefaultHeight);
        map(mTargetProperties->layers, SessionModel::TargetDefaultLayers);
        map(mTargetProperties->samples, SessionModel::TargetDefaultSamples);
        map(mTargetProperties->frontFace, SessionModel::TargetFrontFace);
        map(mTargetProperties->cullMode, SessionModel::TargetCullMode);
        map(mTargetProperties->polygonMode, SessionModel::TargetPolygonMode);
        map(mTargetProperties->logicOperation,
            SessionModel::TargetLogicOperation);
        map(mTargetProperties->blendConstant,
            SessionModel::TargetBlendConstant);
        updateTargetWidgets(index);
        break;

    case Item::Type::Attachment:
        mAttachmentProperties->addMappings(*mMapper);
        break;

    case Item::Type::Call: mCallProperties->addMappings(*mMapper); break;

    case Item::Type::Script:
        map(mScriptProperties->file, SessionModel::FileName);
        map(mScriptProperties->executeOn, SessionModel::ScriptExecuteOn);
        break;

    case Item::Type::AccelerationStructure:
        map(mAccelerationStructureProperties->name, SessionModel::Name);
        break;

    case Item::Type::Instance:
        map(mInstanceProperties->name, SessionModel::Name);
        map(mInstanceProperties->transform, SessionModel::InstanceTransform);
        break;

    case Item::Type::Geometry:
        map(mGeometryProperties->name, SessionModel::Name);
        map(mGeometryProperties->type, SessionModel::GeometryType);
        map(mGeometryProperties->vertexBufferBlock,
            SessionModel::GeometryVertexBufferBlockId);
        map(mGeometryProperties->indexBufferBlock,
            SessionModel::GeometryIndexBufferBlockId);
        map(mGeometryProperties->count, SessionModel::GeometryCount);
        map(mGeometryProperties->offset, SessionModel::GeometryOffset);
        break;
    }

    mMapper->setRootIndex(mModel.parent(index));
    mMapper->setCurrentModelIndex(index);

    // values of Item::Type must match order of Stack Widgets
    static_assert(static_cast<int>(Item::Type::Root) == 0);
    static_assert(static_cast<int>(Item::Type::Geometry) == 18);
    const auto lastStackWidget = static_cast<Item::Type>(mStack->count() - 1);
    Q_ASSERT(lastStackWidget == Item::Type::Geometry);

    const auto stackIndex = static_cast<int>(mModel.getItemType(index));
    mStack->setCurrentIndex(stackIndex);
}

IEditor *PropertiesEditor::openEditor(const FileItem &fileItem)
{
    if (fileItem.fileName.isEmpty()) {
        const auto fileName =
            FileDialog::generateNextUntitledFileName(fileItem.name);
        mModel.setData(mModel.getIndex(&fileItem, SessionModel::FileName),
            fileName);
    }

    auto &editors = Singletons::editorManager();
    switch (fileItem.type) {
    case Item::Type::Texture: {
        if (auto editor = editors.openTextureEditor(fileItem.fileName))
            return editor;
        return editors.openNewTextureEditor(fileItem.fileName);
    }

    case Item::Type::Script: return editors.openEditor(fileItem.fileName);

    case Item::Type::Shader: {
        auto editor = editors.openSourceEditor(fileItem.fileName);
        if (!editor)
            editor = editors.openNewSourceEditor(fileItem.fileName);
        if (auto shader = castItem<Shader>(fileItem)) {
            const auto sourceType =
                getSourceType(shader->shaderType, shader->language);
            if (sourceType != SourceType::PlainText)
                editor->setSourceType(sourceType);
        }
        return editor;
    }

    case Item::Type::Buffer: {
        if (auto editor = editors.openBinaryEditor(fileItem.fileName))
            return editor;
        return editors.openNewBinaryEditor(fileItem.fileName);
    }

    default: return nullptr;
    }
}

IEditor *PropertiesEditor::openItemEditor(const QModelIndex &index)
{
    const auto &item = mModel.getItem(index);

    // open all shaders of program
    if (auto program = castItem<Program>(item)) {
        for (auto item : program->items)
            if (auto shader = castItem<Shader>(item)) {
                openEditor(*shader);
                Singletons::editorManager().setAutoRaise(false);
            }
        Singletons::editorManager().setAutoRaise(true);
        return nullptr;
    }

    // open all attachments of target
    if (auto target = castItem<Target>(item)) {
        for (auto item : target->items) {
            if (auto attachment = castItem<Attachment>(item))
                if (auto texture = castItem<Texture>(
                        mModel.findItem(attachment->textureId))) {
                    openEditor(*texture);
                    Singletons::editorManager().setAutoRaise(false);
                }
        }
        Singletons::editorManager().setAutoRaise(true);
        return nullptr;
    }

    // open first attribute of stream
    if (auto stream = castItem<Stream>(item))
        for (auto item : stream->items)
            if (auto attribute = castItem<Attribute>(item))
                return openItemEditor(mModel.getIndex(attribute));

    // open block of attribute
    if (auto attribute = castItem<Attribute>(item))
        if (auto field = castItem<Field>(mModel.findItem(attribute->fieldId)))
            if (auto block = castItem<Block>(field->parent))
                return openItemEditor(mModel.getIndex(block));

    // open block of field
    if (auto field = castItem<Field>(item))
        return openItemEditor(mModel.getIndex(field->parent));

    // open buffer of block
    if (auto block = castItem<Block>(item)) {
        const auto &buffer = *static_cast<Buffer *>(block->parent);
        auto editor = openItemEditor(mModel.getIndex(&buffer));
        if (auto binaryEditor = static_cast<BinaryEditor *>(editor))
            for (auto i = 0; i < buffer.items.size(); ++i)
                if (buffer.items[i] == block) {
                    binaryEditor->setCurrentBlockIndex(i);
                    break;
                }
        return editor;
    }

    // open item of binding
    if (auto binding = castItem<Binding>(item)) {
        switch (binding->bindingType) {
        case Binding::BindingType::Sampler:
        case Binding::BindingType::Image:
            return openItemEditor(
                mModel.getIndex(mModel.findItem(binding->textureId)));
        case Binding::BindingType::Buffer:
        case Binding::BindingType::TextureBuffer:
            return openItemEditor(
                mModel.getIndex(mModel.findItem(binding->bufferId)));
        case Binding::BindingType::BufferBlock:
            return openItemEditor(
                mModel.getIndex(mModel.findItem(binding->blockId)));
        default: break;
        }
    }

    // open item of call
    if (auto call = castItem<Call>(item)) {
        switch (call->callType) {
        case Call::CallType::Draw:
        case Call::CallType::DrawIndexed:
        case Call::CallType::DrawIndirect:
        case Call::CallType::DrawIndexedIndirect:
        case Call::CallType::DrawMeshTasks:
        case Call::CallType::DrawMeshTasksIndirect:
        case Call::CallType::Compute:
        case Call::CallType::ComputeIndirect:
        case Call::CallType::TraceRays:
            return openItemEditor(
                mModel.getIndex(mModel.findItem(call->programId)));
        case Call::CallType::ClearTexture:
        case Call::CallType::CopyTexture:
        case Call::CallType::SwapTextures:
            return openItemEditor(
                mModel.getIndex(mModel.findItem(call->textureId)));
        case Call::CallType::ClearBuffer:
        case Call::CallType::CopyBuffer:
        case Call::CallType::SwapBuffers:
            return openItemEditor(
                mModel.getIndex(mModel.findItem(call->bufferId)));
        }
    }

    if (const auto fileItem = castItem<FileItem>(item)) {
        auto editor = openEditor(*fileItem);
        Singletons::synchronizeLogic().updateEditor(item.id, true);
        return editor;
    }

    return nullptr;
}

QString PropertiesEditor::currentItemName() const
{
    return mModel.data(currentModelIndex(SessionModel::Name)).toString();
}

QString PropertiesEditor::currentItemFileName() const
{
    return mModel.data(currentModelIndex(SessionModel::FileName)).toString();
}

void PropertiesEditor::switchToCurrentFileItemDirectory()
{
    const auto fileName = currentItemFileName();
    if (!FileDialog::isEmptyOrUntitled(fileName))
        Singletons::fileDialog().setDirectory(QFileInfo(fileName).dir());
}

void PropertiesEditor::setCurrentItemFile(const QString &fileName)
{
    mModel.setData(currentModelIndex(SessionModel::FileName), fileName);
}

void PropertiesEditor::saveCurrentItemFileAs(FileDialog::Options options)
{
    options |= FileDialog::Saving;
    const auto prevFileName = currentItemFileName();
    auto fileName = prevFileName;

    switchToCurrentFileItemDirectory();
    while (Singletons::fileDialog().exec(options, fileName)) {
        fileName = Singletons::fileDialog().fileName();
        if (auto editor = openItemEditor(currentModelIndex())) {
            editor->setFileName(fileName);
            if (!editor->save()) {
                editor->setFileName(prevFileName);
                if (!showSavingFailedMessage(this, fileName))
                    break;
                continue;
            }
            return setCurrentItemFile(fileName);
        }
    }
}

void PropertiesEditor::openCurrentItemFile(FileDialog::Options options)
{
    switchToCurrentFileItemDirectory();
    if (Singletons::fileDialog().exec(options))
        setCurrentItemFile(Singletons::fileDialog().fileName());
}

void PropertiesEditor::updateBlockWidgets(const QModelIndex &index)
{
    auto stride = 0;
    auto isFirstBlock = true;
    auto isLastBlock = true;
    auto hasFile = false;
    if (auto block = mModel.item<Block>(index)) {
        const auto &buffer = *static_cast<Buffer *>(block->parent);
        stride = getBlockStride(*block);
        isFirstBlock = (buffer.items.first() == block);
        isLastBlock = (buffer.items.last() == block);
        if (!buffer.fileName.isEmpty())
            hasFile = true;
    }

    auto &ui = *mBlockProperties;
    ui.stride->setText(QString::number(stride));
    ui.deduceOffset->setVisible(!isFirstBlock);
    ui.deduceRowCount->setVisible(hasFile && isLastBlock);
}

void PropertiesEditor::updateTargetWidgets(const QModelIndex &index)
{
    const auto target = mModel.item<Target>(index);
    const auto hasAttachments = !target->items.empty();

    auto &ui = *mTargetProperties;
    setFormVisibility(ui.formLayout, ui.labelWidth, ui.width, !hasAttachments);
    setFormVisibility(ui.formLayout, ui.labelHeight, ui.height,
        !hasAttachments);
    setFormVisibility(ui.formLayout, ui.labelLayers, ui.layers,
        !hasAttachments);
    setFormVisibility(ui.formLayout, ui.labelSamples, ui.samples,
        !hasAttachments);
    setFormVisibility(ui.formLayout, ui.labelFrontFace, ui.frontFace, true);
    setFormVisibility(ui.formLayout, ui.labelCullMode, ui.cullMode, true);
    setFormVisibility(ui.formLayout, ui.labelPolygonMode, ui.polygonMode, true);
    setFormVisibility(ui.formLayout, ui.labelLogicOperation, ui.logicOperation,
        hasAttachments);
    setFormVisibility(ui.formLayout, ui.labelBlendConstant, ui.blendConstant,
        hasAttachments);
}

void PropertiesEditor::updateGeometryWidgets()
{
    const auto geometryType = static_cast<Geometry::GeometryType>(
        mGeometryProperties->type->currentData().toInt());
    const auto hasIndices =
        (geometryType != Geometry::GeometryType::AxisAlignedBoundingBoxes);

    auto &ui = *mGeometryProperties;
    setFormVisibility(ui.formLayout, ui.labelIndexBufferBlock,
        ui.indexBufferBlock, hasIndices);
}

void PropertiesEditor::deduceBlockOffset()
{
    auto offset = 0;
    const auto &block = *mModel.item<Block>(currentModelIndex());
    for (auto item : std::as_const(block.parent->items)) {
        if (item == &block)
            break;

        const auto &prevBlock = *static_cast<const Block *>(item);
        auto prevOffset = 0, prevRowCount = 0;
        Singletons::synchronizeLogic().evaluateBlockProperties(prevBlock,
            &prevOffset, &prevRowCount);

        offset = std::max(offset, prevOffset)
            + getBlockStride(prevBlock) * prevRowCount;
    }
    mModel.setData(
        mModel.getIndex(currentModelIndex(), SessionModel::BlockOffset),
        offset);
}

void PropertiesEditor::deduceBlockRowCount()
{
    const auto &block = *mModel.item<Block>(currentModelIndex());
    const auto &buffer = *static_cast<const Buffer *>(block.parent);
    const auto stride = getBlockStride(block);
    if (stride) {
        auto offset = 0, rowCount = 0;
        Singletons::synchronizeLogic().evaluateBlockProperties(block, &offset,
            &rowCount);
        auto binary = QByteArray();
        if (Singletons::fileCache().getBinary(buffer.fileName, &binary))
            mModel.setData(mModel.getIndex(currentModelIndex(),
                               SessionModel::BlockRowCount),
                (binary.size() - offset) / stride);
    }
}

void PropertiesEditor::deduceShaderType()
{
    const auto &shader = *mModel.item<Shader>(currentModelIndex());
    const auto fileName = mShaderProperties->file->currentData().toString();
    auto source = QString();
    if (Singletons::fileCache().getSource(fileName, &source)) {
        const auto currentSourceType = (shader.fileName.isEmpty()
                ? SourceType::PlainText
                : getSourceType(shader.shaderType, shader.language));
        const auto extension = FileDialog::getFileExtension(fileName);
        const auto sourceType =
            deduceSourceType(currentSourceType, extension, source);
        if (sourceType != currentSourceType) {
            const auto shaderType = getShaderType(sourceType);
            const auto shaderLanguage = getShaderLanguage(sourceType);
            mModel.setData(
                mModel.getIndex(currentModelIndex(), SessionModel::ShaderType),
                shaderType);
            mModel.setData(mModel.getIndex(currentModelIndex(),
                               SessionModel::ShaderLanguage),
                shaderLanguage);
        }
    }
}
