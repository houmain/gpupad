#include "SessionProperties.h"
#include "TextureProperties.h"
#include "BindingProperties.h"
#include "AttachmentProperties.h"
#include "CallProperties.h"
#include "ui_GroupProperties.h"
#include "ui_BufferProperties.h"
#include "ui_ColumnProperties.h"
#include "ui_ImageProperties.h"
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

class StackedWidget : public QStackedWidget
{
public:
    using QStackedWidget::QStackedWidget;

    QSize minimumSizeHint() const
    {
        return currentWidget()->minimumSizeHint();
    }
};

QString splitPascalCase(QString str)
{
    return str.replace(QRegularExpression("([a-z])([A-Z])"), "\\1 \\2");
}

template <typename T>
void instantiate(QScopedPointer<T> &ptr) { ptr.reset(new T()); }

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
    add(mImageProperties);
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
    using Overload = void(QSpinBox::*)(int);
    connect(mBufferProperties->rowCount, static_cast<Overload>(&QSpinBox::valueChanged),
        [this]() { updateBufferWidgets(currentModelIndex()); });
    connect(mBufferProperties->size, &QSpinBox::editingFinished,
        this, &SessionProperties::deduceBufferRowCount);
    connect(mBufferProperties->deduceRowCount, &QToolButton::clicked,
        this, &SessionProperties::deduceBufferRowCount);

    connect(mImageProperties->fileNew, &QToolButton::clicked,
        [this]() { saveCurrentItemFileAs(FileDialog::ImageExtensions); });
    connect(mImageProperties->fileBrowse, &QToolButton::clicked,
        [this]() { openCurrentItemFile(FileDialog::ImageExtensions); });
    connect(mImageProperties->file, &ReferenceComboBox::listRequired,
        [this]() { return getFileNames(Item::Type::Image, true); });

    connect(mScriptProperties->fileNew, &QToolButton::clicked,
        [this]() { saveCurrentItemFileAs(FileDialog::ScriptExtensions); });
    connect(mScriptProperties->fileBrowse, &QToolButton::clicked,
        [this]() { openCurrentItemFile(FileDialog::ScriptExtensions); });
    connect(mScriptProperties->file, &ReferenceComboBox::listRequired,
        [this]() { return getFileNames(Item::Type::Script); });

    connect(mAttributeProperties->buffer, &ReferenceComboBox::currentDataChanged,
        mAttributeProperties->column, &ReferenceComboBox::validate);
    connect(mAttributeProperties->column, &ReferenceComboBox::listRequired,
        [this]() { return getColumnIds(
            mAttributeProperties->buffer->currentData().toInt()); });
    connect(mAttributeProperties->buffer, &ReferenceComboBox::listRequired,
        [this]() { return getItemIds(Item::Type::Buffer); });

    for (auto comboBox : { mShaderProperties->file, mBufferProperties->file,
            mImageProperties->file, mScriptProperties->file })
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
    fillComboBox<Image::CubeMapFace>(mImageProperties->face);
    fillComboBox<Shader::ShaderType>(mShaderProperties->type);
}

QVariantList SessionProperties::getFileNames(Item::Type type, bool addNull) const
{
    auto result = QVariantList();
    if (addNull)
        result += "";

    switch (type) {
        case Item::Type::Shader:
        case Item::Type::Script:
            foreach (QString f, Singletons::editorManager().getSourceFileNames())
                if (!result.contains(f))
                    result.append(f);
            break;

        case Item::Type::Buffer:
            foreach (QString f, Singletons::editorManager().getBinaryFileNames())
                if (!result.contains(f))
                    result.append(f);
            break;

        case Item::Type::Texture:
        case Item::Type::Image:
            foreach (QString f, Singletons::editorManager().getImageFileNames())
                if (!result.contains(f))
                    result.append(f);
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
    qSort(result);

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

        case Item::Type::Image:
            map(mImageProperties->file, SessionModel::FileName);
            map(mImageProperties->level, SessionModel::ImageLevel);
            map(mImageProperties->layer, SessionModel::ImageLayer);
            map(mImageProperties->face, SessionModel::ImageFace);
            updateImageWidgets(index);
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
            map(mTargetProperties->frontFace, SessionModel::TargetFrontFace);
            map(mTargetProperties->cullMode, SessionModel::TargetCullMode);
            map(mTargetProperties->logicOperation, SessionModel::TargetLogicOperation);
            map(mTargetProperties->blendConstant, SessionModel::TargetBlendConstant);
            break;

        case Item::Type::Attachment:
            mAttachmentProperties->addMappings(*mMapper);
            break;

        case Item::Type::Call:
            mCallProperties->addMappings(*mMapper);
            break;

        case Item::Type::Script:
            map(mScriptProperties->file, SessionModel::FileName);
            break;
    }

    mMapper->setRootIndex(mModel.parent(index));
    mMapper->setCurrentModelIndex(index);

    mStack->setCurrentIndex(static_cast<int>(mModel.getItemType(index)));
}

IEditor* SessionProperties::openItemEditor(const QModelIndex &index)
{
    auto &editors = Singletons::editorManager();
    if (auto fileItem = mModel.item<FileItem>(index)) {
        switch (fileItem->type) {
            case Item::Type::Texture:
                if (!fileItem->items.isEmpty())
                    return nullptr;
                // fallthrough
            case Item::Type::Image:
                if (fileItem->fileName.isEmpty())
                    mModel.setData(mModel.getIndex(fileItem, SessionModel::FileName),
                        editors.openNewImageEditor(fileItem->name));
                return editors.openImageEditor(fileItem->fileName);

            case Item::Type::Shader: {
                if (fileItem->fileName.isEmpty())
                    mModel.setData(mModel.getIndex(fileItem, SessionModel::FileName),
                        editors.openNewSourceEditor(fileItem->name));
                auto editor = editors.openSourceEditor(fileItem->fileName);
                if (auto shader = mModel.item<Shader>(index)) {
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
                return editor;
            }

            case Item::Type::Script: {
                if (fileItem->fileName.isEmpty())
                    mModel.setData(mModel.getIndex(fileItem, SessionModel::FileName),
                        editors.openNewSourceEditor(fileItem->name));
                auto editor = editors.openSourceEditor(fileItem->fileName);
                editor->setSourceType(SourceType::JavaScript);
                return editor;
            }

            case Item::Type::Buffer:
                if (fileItem->fileName.isEmpty())
                    mModel.setData(mModel.getIndex(fileItem, SessionModel::FileName),
                        editors.openNewBinaryEditor(fileItem->name));

                if (auto editor = editors.openBinaryEditor(fileItem->fileName)) {
                    Singletons::synchronizeLogic().updateBinaryEditor(
                        static_cast<const Buffer&>(*fileItem), *editor, true);
                    return editor;
                }
                break;

            default:
                break;
        }
    }
    return nullptr;
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
    options |= FileDialog::Loading;
    if (Singletons::fileDialog().exec(options))
        setCurrentItemFile(Singletons::fileDialog().fileName());
}

void SessionProperties::updateImageWidgets(const QModelIndex &index)
{
    auto kind = TextureKind();
    if (auto image = mModel.item<Image>(index))
        if (auto texture = castItem<Texture>(image->parent))
            kind = getKind(*texture);

    auto &ui = *mImageProperties;
    setFormVisibility(ui.formLayout, ui.labelLayer, ui.layer, kind.array);
    setFormVisibility(ui.formLayout, ui.labelFace, ui.face, kind.cubeMap);
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

