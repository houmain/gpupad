#include "SessionProperties.h"
#include "TextureProperties.h"
#include "BindingProperties.h"
#include "AttachmentProperties.h"
#include "CallProperties.h"
#include "ui_GroupProperties.h"
#include "ui_BufferProperties.h"
#include "ui_ColumnProperties.h"
#include "ui_ProgramProperties.h"
#include "ui_ShaderProperties.h"
#include "ui_StreamProperties.h"
#include "ui_AttributeProperties.h"
#include "ui_TargetProperties.h"
#include "ui_ScriptProperties.h"
#include "editors/EditorManager.h"
#include "Singletons.h"
#include "SessionModel.h"
#include "SynchronizeLogic.h"
#include "FileCache.h"
#include <QStackedWidget>
#include <QDataWidgetMapper>
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
  void instantiate(QScopedPointer<T> &ptr)
  {
      ptr.reset(new T());
  }
} // namespace

QString splitPascalCase(QString str)
{
    return str.replace(QRegularExpression("([a-z])([A-Z])"), "\\1 \\2");
}

void setFormVisibility(QFormLayout *layout, QLabel *label,
    QWidget *widget, bool visible)
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

void setFormEnabled(QLabel *label,
    QWidget* widget, bool enabled)
{
    label->setEnabled(enabled);
    widget->setEnabled(enabled);
}

SessionProperties::SessionProperties(QWidget *parent)
    : QScrollArea(parent)
    , mModel(Singletons::sessionModel())
    , mStack(new StackedWidget(this))
    , mMapper(new QDataWidgetMapper(this))
    , mSubmitTimer(new QTimer(this))
{
    setFrameShape(QFrame::NoFrame);
    setBackgroundRole(QPalette::ToolTipBase);

    mMapper->setModel(&mModel);
    connect(mSubmitTimer, &QTimer::timeout,
        mMapper, &QDataWidgetMapper::submit);
    mSubmitTimer->start(100);

    const auto add = [&](auto &ui) {
        auto widget = new QWidget(this);
        instantiate(ui);
        ui->setupUi(widget);
        mStack->addWidget(widget);
    };
    add(mGroupProperties);
    add(mBufferProperties);
    add(mColumnProperties);
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
    mStack->addWidget(new QWidget(this));

    setWidgetResizable(true);
    setWidget(mStack);

    connect(mShaderProperties->fileNew, &QToolButton::clicked,
        [this]() { saveCurrentItemFileAs(FileDialog::ShaderExtensions); });
    connect(mShaderProperties->fileBrowse, &QToolButton::clicked,
        [this]() { openCurrentItemFile(FileDialog::ShaderExtensions); });
    connect(mShaderProperties->file, &ReferenceComboBox::listRequired,
        [this]() { return getFileNames(Item::Type::Shader); });

    connect(mBufferProperties->fileNew, &QToolButton::clicked,
        [this]() { saveCurrentItemFileAs(FileDialog::BinaryExtensions); });
    connect(mBufferProperties->fileBrowse, &QToolButton::clicked,
        [this]() { openCurrentItemFile(FileDialog::BinaryExtensions); });
    connect(mBufferProperties->file, &ReferenceComboBox::listRequired,
        [this]() { return getFileNames(Item::Type::Buffer, true); });
    connect(mBufferProperties->rowCount, qOverload<int>(&QSpinBox::valueChanged),
        [this]() { updateBufferWidgets(currentModelIndex()); });
    connect(mBufferProperties->size, &QSpinBox::editingFinished,
        this, &SessionProperties::deduceBufferRowCount);
    connect(mBufferProperties->deduceRowCount, &QToolButton::clicked,
        this, &SessionProperties::deduceBufferRowCount);

    connect(mScriptProperties->fileNew, &QToolButton::clicked,
        [this]() { saveCurrentItemFileAs(FileDialog::ScriptExtensions); });
    connect(mScriptProperties->fileBrowse, &QToolButton::clicked,
        [this]() { openCurrentItemFile(FileDialog::ScriptExtensions); });
    connect(mScriptProperties->file, &ReferenceComboBox::listRequired,
        [this]() { return getFileNames(Item::Type::Script, true); });
    connect(mScriptProperties->file, &ReferenceComboBox::currentDataChanged,
        [this](QVariant data) { updateScriptWidgets(!data.toString().isEmpty()); });

    connect(mAttributeProperties->buffer, &ReferenceComboBox::currentDataChanged,
        mAttributeProperties->column, &ReferenceComboBox::validate);
    connect(mAttributeProperties->column, &ReferenceComboBox::listRequired,
        [this]() { return getColumnIds(
            mAttributeProperties->buffer->currentData().toInt()); });
    connect(mAttributeProperties->buffer, &ReferenceComboBox::listRequired,
        [this]() { return getItemIds(Item::Type::Buffer); });

    for (auto comboBox : { mShaderProperties->file, mBufferProperties->file, mScriptProperties->file })
        connect(comboBox, &ReferenceComboBox::textRequired,
            [](auto data) { return FileDialog::getFileTitle(data.toString()); });

    for (auto comboBox : { mAttributeProperties->buffer, mAttributeProperties->column })
        connect(comboBox, &ReferenceComboBox::textRequired,
            [this](QVariant data) { return findItemName(data.toInt()); });

    setCurrentModelIndex({ });
    fillComboBoxes();
}

SessionProperties::~SessionProperties() = default;

void SessionProperties::fillComboBoxes()
{
    fillComboBox<Column::DataType>(mColumnProperties->type);
    fillComboBox<Target::FrontFace>(mTargetProperties->frontFace);
    fillComboBox<Target::CullMode>(mTargetProperties->cullMode);
    fillComboBox<Target::LogicOperation>(mTargetProperties->logicOperation);
    fillComboBox<Shader::ShaderType>(mShaderProperties->type);
    fillComboBox<Script::ExecuteOn>(mScriptProperties->executeOn);
}

QVariantList SessionProperties::getFileNames(Item::Type type, bool addNull) const
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

        default:
            break;
    }

    mModel.forEachItem([&](const Item &item) {
        if (item.type == type) {
            const auto &fileName = static_cast<const FileItem&>(item).fileName;
            if (!result.contains(fileName))
                result.append(fileName);
        }
    });
    return result;
}

QString SessionProperties::findItemName(ItemId itemId) const
{
    return mModel.findItemName(itemId);
}

QVariantList SessionProperties::getItemIds(Item::Type type, bool addNull) const
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

QVariantList SessionProperties::getColumnIds(ItemId bufferId) const
{
    auto result = QVariantList();
    mModel.forEachItem([&](const Item &item) {
        if (item.type == Item::Type::Column)
            if (static_cast<const Buffer*>(item.parent)->id == bufferId)
                result.append(item.id);
    });
    return result;
}

void SessionProperties::updateModel()
{
    mMapper->submit();
}

QModelIndex SessionProperties::currentModelIndex(int column) const
{
    return mModel.index(mMapper->currentIndex(), column, mMapper->rootIndex());
}

void SessionProperties::setCurrentModelIndex(const QModelIndex &index)
{
    mMapper->submit();
    mMapper->clearMapping();

    if (!index.isValid()) {
        mStack->setCurrentIndex(mStack->count() - 1);
        setVisible(false);
        return;
    }
    setVisible(true);

    const auto map = [&](QWidget *control, SessionModel::ColumnType column) {
        mMapper->addMapping(control, column);
    };

    switch (mModel.getItemType(index)) {
        case Item::Type::Group:
            map(mGroupProperties->inlineScope, SessionModel::GroupInlineScope);
            break;

        case Item::Type::Buffer:
            map(mBufferProperties->file, SessionModel::FileName);
            map(mBufferProperties->offset, SessionModel::BufferOffset);
            map(mBufferProperties->rowCount, SessionModel::BufferRowCount);
            updateBufferWidgets(index);
            break;

        case Item::Type::Column:
            map(mColumnProperties->type, SessionModel::ColumnDataType);
            map(mColumnProperties->count, SessionModel::ColumnCount);
            map(mColumnProperties->padding, SessionModel::ColumnPadding);
            break;

        case Item::Type::Texture:
            mTextureProperties->addMappings(*mMapper);
            break;

        case Item::Type::Program:
            break;

        case Item::Type::Shader:
            map(mShaderProperties->type, SessionModel::ShaderType);
            map(mShaderProperties->file, SessionModel::FileName);
            break;

        case Item::Type::Binding:
            mBindingProperties->addMappings(*mMapper);
            break;

        case Item::Type::Stream:
            break;

        case Item::Type::Attribute:
            map(mAttributeProperties->buffer, SessionModel::AttributeBufferId);
            map(mAttributeProperties->column, SessionModel::AttributeColumnId);
            map(mAttributeProperties->normalize, SessionModel::AttributeNormalize);
            map(mAttributeProperties->divisor, SessionModel::AttributeDivisor);
            break;

        case Item::Type::Target:
            map(mTargetProperties->width, SessionModel::TargetDefaultWidth);
            map(mTargetProperties->height, SessionModel::TargetDefaultHeight);
            map(mTargetProperties->layers, SessionModel::TargetDefaultLayers);
            map(mTargetProperties->samples, SessionModel::TargetDefaultSamples);
            map(mTargetProperties->frontFace, SessionModel::TargetFrontFace);
            map(mTargetProperties->cullMode, SessionModel::TargetCullMode);
            map(mTargetProperties->logicOperation, SessionModel::TargetLogicOperation);
            map(mTargetProperties->blendConstant, SessionModel::TargetBlendConstant);
            updateTargetWidgets(index);
            break;

        case Item::Type::Attachment:
            mAttachmentProperties->addMappings(*mMapper);
            break;

        case Item::Type::Call:
            mCallProperties->addMappings(*mMapper);
            break;

        case Item::Type::Script:
            map(mScriptProperties->file, SessionModel::FileName);
            map(mScriptProperties->executeOn, SessionModel::ScriptExecuteOn);
            map(mScriptProperties->expression, SessionModel::ScriptExpression);
            updateScriptWidgets(index);
            break;
    }

    mMapper->setRootIndex(mModel.parent(index));
    mMapper->setCurrentModelIndex(index);

    mStack->setCurrentIndex(static_cast<int>(mModel.getItemType(index)));
}

IEditor* SessionProperties::openEditor(const FileItem &fileItem)
{
    auto &editors = Singletons::editorManager();
    switch (fileItem.type) {
        case Item::Type::Texture:
            if (fileItem.fileName.isEmpty())
                mModel.setData(mModel.getIndex(&fileItem, SessionModel::FileName),
                    editors.openNewTextureEditor(fileItem.name));
            return editors.openTextureEditor(fileItem.fileName);

        case Item::Type::Shader:
        case Item::Type::Script:
            if (fileItem.fileName.isEmpty())
                mModel.setData(mModel.getIndex(&fileItem, SessionModel::FileName),
                    editors.openNewSourceEditor(fileItem.name));
            return editors.openSourceEditor(fileItem.fileName);

        case Item::Type::Buffer:
            if (fileItem.fileName.isEmpty())
                mModel.setData(mModel.getIndex(&fileItem, SessionModel::FileName),
                    editors.openNewBinaryEditor(fileItem.name));
            return editors.openBinaryEditor(fileItem.fileName);

        default:
            return nullptr;
    }
}

IEditor* SessionProperties::openItemEditor(const QModelIndex &index)
{
    const auto& item = mModel.getItem(index);

    if (auto script = castItem<Script>(item))
        if (!script->expression.isEmpty())
            return nullptr;

    auto editor = std::add_pointer_t<IEditor>();
    if (auto program = castItem<Program>(item)) {
        for (auto item : program->items)
            if (auto shader = castItem<Shader>(item))
                editor = openEditor(*shader);
        return editor;
    }

    if (const auto fileItem = castItem<FileItem>(item))
        editor = openEditor(*fileItem);
    if (!editor)
        return nullptr;

    if (auto script = castItem<Script>(item)) {
        editor->setSourceType(SourceType::JavaScript);
    }
    else if (auto shader = castItem<Shader>(item)) {
        static const auto sMapping = QMap<Shader::ShaderType, SourceType>{
            { Shader::ShaderType::Vertex, SourceType::VertexShader },
            { Shader::ShaderType::Fragment, SourceType::FragmentShader },
            { Shader::ShaderType::Geometry, SourceType::GeometryShader },
            { Shader::ShaderType::TessellationControl, SourceType::TesselationControl },
            { Shader::ShaderType::TessellationEvaluation, SourceType::TesselationEvaluation },
            { Shader::ShaderType::Compute, SourceType::ComputeShader },
        };
        auto sourceType = sMapping[shader->shaderType];
        if (sourceType != SourceType::None)
            editor->setSourceType(sourceType);
    }
    else {
        Singletons::synchronizeLogic().updateEditor(item.id, true);
    }
    return editor;
}

QString SessionProperties::currentItemName() const
{
    return mModel.data(currentModelIndex(SessionModel::Name)).toString();
}

QString SessionProperties::currentItemFileName() const
{
    return mModel.data(currentModelIndex(SessionModel::FileName)).toString();
}

void SessionProperties::setCurrentItemFile(const QString &fileName)
{
    mModel.setData(currentModelIndex(SessionModel::FileName), fileName);
}

void SessionProperties::saveCurrentItemFileAs(FileDialog::Options options)
{
    options |= FileDialog::Saving;
    if (Singletons::fileDialog().exec(options, currentItemFileName())) {
        auto fileName = Singletons::fileDialog().fileName();
        if (auto editor = openItemEditor(currentModelIndex())) {
            editor->setFileName(fileName);
            setCurrentItemFile(fileName);
            editor->save();
        }
    }
}

void SessionProperties::openCurrentItemFile(FileDialog::Options options)
{
    if (Singletons::fileDialog().exec(options))
        setCurrentItemFile(Singletons::fileDialog().fileName());
}

void SessionProperties::updateBufferWidgets(const QModelIndex &index)
{
    auto stride = 0;
    if (auto buffer = mModel.item<Buffer>(index))
        stride = getStride(*buffer);

    auto &ui = *mBufferProperties;
    ui.stride->setText(QString::number(stride));
    ui.size->setValue(stride * ui.rowCount->value());
    setFormVisibility(ui.formLayout, ui.labelRows, ui.widgetRows, stride > 0);
    setFormVisibility(ui.formLayout, ui.labelSize, ui.size, stride > 0);
}

void SessionProperties::updateTargetWidgets(const QModelIndex &index)
{
    const auto target = mModel.item<Target>(index);
    const auto hasAttachments = !target->items.empty();

    auto &ui = *mTargetProperties;
    setFormVisibility(ui.formLayout, ui.labelWidth, ui.width, !hasAttachments);
    setFormVisibility(ui.formLayout, ui.labelHeight, ui.height, !hasAttachments);
    setFormVisibility(ui.formLayout, ui.labelLayers, ui.layers, !hasAttachments);
    setFormVisibility(ui.formLayout, ui.labelSamples, ui.samples, !hasAttachments);
    setFormVisibility(ui.formLayout, ui.labelFrontFace, ui.frontFace, true);
    setFormVisibility(ui.formLayout, ui.labelCullMode, ui.cullMode, true);
    setFormVisibility(ui.formLayout, ui.labelLogicOperation, ui.logicOperation, hasAttachments);
    setFormVisibility(ui.formLayout, ui.labelBlendConstant, ui.blendConstant, hasAttachments);
}

void SessionProperties::updateScriptWidgets(const QModelIndex &index)
{
    auto hasFile = false;
    if (auto script = mModel.item<Script>(index))
        hasFile = !script->fileName.isEmpty();
    updateScriptWidgets(hasFile);
}

void SessionProperties::updateScriptWidgets(bool hasFile)
{
    auto &ui = *mScriptProperties;
    setFormVisibility(ui.formLayout, ui.labelExpression, ui.expression, !hasFile);
}

void SessionProperties::deduceBufferRowCount()
{
    if (auto buffer = mModel.item<Buffer>(currentModelIndex())) {
        auto size = mBufferProperties->size->value();
        if (QObject::sender() == mBufferProperties->deduceRowCount) {
            auto binary = QByteArray();
            if (!Singletons::fileCache().getBinary(buffer->fileName, &binary))
                return;
            size = binary.size() - buffer->offset;
        }

        if (auto stride = getStride(*buffer)) {
            mModel.setData(mModel.getIndex(currentModelIndex(),
                    SessionModel::BufferRowCount), size / stride);
            updateBufferWidgets(currentModelIndex());
        }
    }
}

