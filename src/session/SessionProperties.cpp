#include "SessionProperties.h"
#include "TextureProperties.h"
#include "BindingProperties.h"
#include "AttachmentProperties.h"
#include "CallProperties.h"
#include "ui_GroupProperties.h"
#include "ui_BufferProperties.h"
#include "ui_ColumnProperties.h"
#include "ui_ImageProperties.h"
#include "ui_SamplerProperties.h"
#include "ui_ProgramProperties.h"
#include "ui_ShaderProperties.h"
#include "ui_VertexStreamProperties.h"
#include "ui_AttributeProperties.h"
#include "ui_TargetProperties.h"
#include "ui_ScriptProperties.h"
#include "editors/EditorManager.h"
#include "Singletons.h"
#include "SessionModel.h"
#include <QStackedWidget>
#include <QDataWidgetMapper>
#include <QTimer>

class StackedWidget : public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;

    QSize minimumSizeHint() const
    {
        return currentWidget()->minimumSizeHint();
    }
};

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
    add(mSamplerProperties);
    add(mProgramProperties);
    add(mShaderProperties);
    mBindingProperties = new BindingProperties(this);
    mStack->addWidget(mBindingProperties);
    add(mVertexStreamProperties);
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

    connect(mShaderProperties->fileNew, &QToolButton::clicked, [this]() {
        setCurrentItemFile(Singletons::editorManager().openNewSourceEditor(
            currentItemName())); });
    connect(mShaderProperties->fileBrowse, &QToolButton::clicked,
        [this]() { selectCurrentItemFile(FileDialog::ShaderExtensions); });
    connect(mShaderProperties->file, &ReferenceComboBox::listRequired,
        [this]() { return getFileNames(ItemType::Shader); });

    connect(mBufferProperties->fileNew, &QToolButton::clicked, [this]() {
        setCurrentItemFile(Singletons::editorManager().openNewBinaryEditor(
            currentItemName())); });
    connect(mBufferProperties->fileBrowse, &QToolButton::clicked,
        [this]() { selectCurrentItemFile(FileDialog::BinaryExtensions); });
    connect(mBufferProperties->file, &ReferenceComboBox::listRequired,
        [this]() { return getFileNames(ItemType::Buffer); });

    connect(mImageProperties->fileNew, &QToolButton::clicked, [this]() {
        setCurrentItemFile(Singletons::editorManager().openNewImageEditor(
            currentItemName())); });
    connect(mImageProperties->fileBrowse, &QToolButton::clicked,
        [this]() { selectCurrentItemFile(FileDialog::ImageExtensions); });
    connect(mImageProperties->file, &ReferenceComboBox::listRequired,
        [this]() { return getFileNames(ItemType::Image, true); });

    connect(mScriptProperties->fileNew, &QToolButton::clicked, [this]() {
        setCurrentItemFile(Singletons::editorManager().openNewSourceEditor(
            currentItemName())); });
    connect(mScriptProperties->fileBrowse, &QToolButton::clicked,
        [this]() { selectCurrentItemFile(FileDialog::ScriptExtensions); });
    connect(mScriptProperties->file, &ReferenceComboBox::listRequired,
        [this]() { return getFileNames(ItemType::Script); });

    connect(mSamplerProperties->texture, &ReferenceComboBox::listRequired,
        [this]() { return getItemIds(ItemType::Texture); });
    connect(mAttributeProperties->buffer, &ReferenceComboBox::listRequired,
        [this]() { return getItemIds(ItemType::Buffer); });
    connect(mAttributeProperties->buffer, &ReferenceComboBox::currentDataChanged,
        mAttributeProperties->column, &ReferenceComboBox::validate);
    connect(mAttributeProperties->column, &ReferenceComboBox::listRequired,
        [this]() { return getColumnIds(
            mAttributeProperties->buffer->currentData().toInt()); });

    for (auto comboBox : { mShaderProperties->file, mBufferProperties->file,
            mImageProperties->file, mScriptProperties->file })
        connect(comboBox, &ReferenceComboBox::textRequired,
            [](auto data) { return FileDialog::getFileTitle(data.toString()); });

    for (auto comboBox : { mSamplerProperties->texture, mAttributeProperties->buffer,
            mAttributeProperties->column })
        connect(comboBox, &ReferenceComboBox::textRequired,
            [this](QVariant data) { return findItemName(data.toInt()); });

    setCurrentModelIndex({ });
    fillComboBoxes();
}

SessionProperties::~SessionProperties() = default;

void SessionProperties::fillComboBoxes()
{
    fill<Column::DataType>(mColumnProperties->type, {
        { "Int8", Column::Int8 },
        { "Int16", Column::Int16 },
        { "Int32", Column::Int32 },
        { "Uint8", Column::Uint8 },
        { "Uint16", Column::Uint16 },
        { "Uint32", Column::Uint32 },
        { "Float", Column::Float },
        { "Double", Column::Double },
    });

    fill<Target::FrontFace>(mTargetProperties->frontFace, {
        { "Counter Clockwise", Target::CCW },
        { "Clockwise", Target::CW },
    });

    fill<Target::CullMode>(mTargetProperties->cullMode, {
        { "Disabled", Target::CullDisabled },
        { "Back", Target::Back },
        { "Front", Target::Front },
        { "Front And Back", Target::FrontAndBack },
    });

    fill<Target::LogicOperation>(mTargetProperties->logicOperation, {
        { "Disabled", Target::LogicOperationDisabled },
        { "Copy", Target::Copy },
        { "Clear", Target::Clear },
        { "Set", Target::Set },
        { "Copy Inverted", Target::CopyInverted },
        { "No Operation", Target::NoOp },
        { "Invert", Target::Invert },
        { "AND", Target::And },
        { "NAND", Target::Nand },
        { "OR", Target::Or },
        { "NOR", Target::Nor },
        { "XOR", Target::Xor },
        { "EQUIV", Target::Equiv },
        { "AND Reverse", Target::AndReverse },
        { "AND Inverted", Target::AndInverted },
        { "OR Reverse", Target::OrReverse },
        { "OR Inverted", Target::OrInverted },
    });

    fill<QOpenGLTexture::Filter>(mSamplerProperties->minFilter, {
        { "Nearest", QOpenGLTexture::Nearest },
        { "Linear", QOpenGLTexture::Linear },
        { "Nearest MipMap-Nearest", QOpenGLTexture::NearestMipMapNearest },
        { "Nearest MipMap-Linear", QOpenGLTexture::NearestMipMapLinear },
        { "Linear MipMap-Nearest", QOpenGLTexture::LinearMipMapNearest },
        { "Linear MipMap-Linear", QOpenGLTexture::LinearMipMapLinear },
    });

    fill<QOpenGLTexture::Filter>(mSamplerProperties->magFilter, {
        { "Nearest", QOpenGLTexture::Nearest },
        { "Linear", QOpenGLTexture::Linear },
    });

    fill<QOpenGLTexture::WrapMode>(mSamplerProperties->wrapModeX, {
        { "Repeat", QOpenGLTexture::Repeat },
        { "Mirrored Repeat", QOpenGLTexture::MirroredRepeat },
        { "Clamp to Edge", QOpenGLTexture::ClampToEdge },
        { "Clamp to Border", QOpenGLTexture::ClampToBorder },
    });
    mSamplerProperties->wrapModeY->setModel(
        mSamplerProperties->wrapModeX->model());
    mSamplerProperties->wrapModeZ->setModel(
        mSamplerProperties->wrapModeX->model());

    fill<QOpenGLTexture::CubeMapFace>(mImageProperties->face, {
        { "Positive X", QOpenGLTexture::CubeMapPositiveX },
        { "Negative X", QOpenGLTexture::CubeMapNegativeX },
        { "Positive Y", QOpenGLTexture::CubeMapPositiveY },
        { "Negative Y", QOpenGLTexture::CubeMapNegativeY },
        { "Positive Z", QOpenGLTexture::CubeMapPositiveZ },
        { "Negative Z", QOpenGLTexture::CubeMapNegativeZ },
    });

    fill<Shader::Type>(mShaderProperties->type, {
        { "Vertex", Shader::Vertex },
        { "Fragment", Shader::Fragment },
        { "Geometry", Shader::Geometry },
        { "Tessellation Control", Shader::TessellationControl },
        { "Tessellation Evaluation", Shader::TessellationEvaluation },
        { "Compute", Shader::Compute },
        { "Header", Shader::Header },
    });
}

QVariantList SessionProperties::getFileNames(ItemType type, bool addNull) const
{
    auto result = QVariantList();
    if (addNull)
        result += "";

    switch (type) {
        case ItemType::Shader:
        case ItemType::Script:
            foreach (QString f, Singletons::editorManager().getSourceFileNames())
                if (!result.contains(f))
                    result.append(f);
            break;

        case ItemType::Buffer:
            foreach (QString f, Singletons::editorManager().getBinaryFileNames())
                if (!result.contains(f))
                    result.append(f);
            break;

        case ItemType::Texture:
        case ItemType::Image:
            foreach (QString f, Singletons::editorManager().getImageFileNames())
                if (!result.contains(f))
                    result.append(f);
            break;

        default:
            break;
    }

    mModel.forEachItem([&](const Item &item) {
        if (item.itemType == type) {
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

QVariantList SessionProperties::getItemIds(ItemType type, bool addNull) const
{
    auto result = QVariantList();
    if (addNull)
        result += 0;

    mModel.forEachItemScoped(currentModelIndex(), [&](const Item &item) {
        if (item.itemType == type)
            result.append(item.id);
    });
    return result;
}

QVariantList SessionProperties::getColumnIds(ItemId bufferId) const
{
    auto result = QVariantList();
    mModel.forEachItem([&](const Item &item) {
        if (item.itemType == ItemType::Column)
            if (static_cast<const Buffer*>(item.parent)->id == bufferId)
                result.append(item.id);
    });
    return result;
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
        case ItemType::Group:
            map(mGroupProperties->inlineScope, SessionModel::InlineScope);
            break;

        case ItemType::Buffer:
            map(mBufferProperties->file, SessionModel::FileName);
            map(mBufferProperties->offset, SessionModel::BufferOffset);
            map(mBufferProperties->rowCount, SessionModel::BufferRowCount);
            break;

        case ItemType::Column:
            map(mColumnProperties->type, SessionModel::ColumnDataType);
            map(mColumnProperties->count, SessionModel::ColumnCount);
            map(mColumnProperties->padding, SessionModel::ColumnPadding);
            break;

        case ItemType::Texture:
            mTextureProperties->addMappings(*mMapper);
            break;

        case ItemType::Image:
            map(mImageProperties->file, SessionModel::FileName);
            map(mImageProperties->level, SessionModel::ImageLevel);
            map(mImageProperties->layer, SessionModel::ImageLayer);
            map(mImageProperties->face, SessionModel::ImageFace);
            updateImageWidgets(index);
            break;

        case ItemType::Sampler:
            map(mSamplerProperties->texture, SessionModel::SamplerTextureId);
            map(mSamplerProperties->minFilter, SessionModel::SamplerMinFilter);
            map(mSamplerProperties->magFilter, SessionModel::SamplerMagFilter);
            map(mSamplerProperties->wrapModeX, SessionModel::SamplerWarpModeX);
            map(mSamplerProperties->wrapModeY, SessionModel::SamplerWarpModeY);
            map(mSamplerProperties->wrapModeZ, SessionModel::SamplerWarpModeZ);
            break;

        case ItemType::Program:
            break;

        case ItemType::Shader:
            map(mShaderProperties->type, SessionModel::ShaderType);
            map(mShaderProperties->file, SessionModel::FileName);
            break;

        case ItemType::Binding:
            // TODO: move to BindingProperties class, remove widget getters
            map(mBindingProperties->typeWidget(), SessionModel::BindingType);
            map(mBindingProperties->editorWidget(), SessionModel::BindingEditor);
            map(mBindingProperties, SessionModel::BindingValues);
            break;

        case ItemType::VertexStream:
            break;

        case ItemType::Attribute:
            map(mAttributeProperties->buffer, SessionModel::AttributeBufferId);
            map(mAttributeProperties->column, SessionModel::AttributeColumnId);
            map(mAttributeProperties->normalize, SessionModel::AttributeNormalize);
            map(mAttributeProperties->divisor, SessionModel::AttributeDivisor);
            break;

        case ItemType::Target:
            map(mTargetProperties->frontFace, SessionModel::TargetFrontFace);
            map(mTargetProperties->cullMode, SessionModel::TargetCullMode);
            map(mTargetProperties->logicOperation, SessionModel::TargetLogicOperation);
            map(mTargetProperties->blendConstant, SessionModel::TargetBlendConstant);
            break;

        case ItemType::Attachment:
            mAttachmentProperties->addMappings(*mMapper);
            break;

        case ItemType::Call:
            mCallProperties->addMappings(*mMapper);
            break;

        case ItemType::Script:
            map(mScriptProperties->file, SessionModel::FileName);
            break;
    }

    mMapper->setRootIndex(mModel.parent(index));
    mMapper->setCurrentModelIndex(index);

    mStack->setCurrentIndex(static_cast<int>(mModel.getItemType(index)));
}

QString SessionProperties::currentItemName() const
{
    return mModel.data(currentModelIndex(SessionModel::Name)).toString();
}

void SessionProperties::selectCurrentItemFile(FileDialog::Options options)
{
    options |= FileDialog::Loading;
    if (Singletons::fileDialog().exec(options))
        setCurrentItemFile(Singletons::fileDialog().fileName());
}

void SessionProperties::setCurrentItemFile(const QString &fileName)
{
    mModel.setData(currentModelIndex(SessionModel::FileName), fileName);
}

void SessionProperties::updateImageWidgets(const QModelIndex &index)
{
    if (auto image = mModel.item<Image>(index))
        if (auto texture = castItem<Texture>(image->parent)) {
            const auto isArray =
                (texture->target == Texture::Target::Target1DArray ||
                 texture->target == Texture::Target::Target2DArray ||
                 texture->target == Texture::Target::Target2DMultisampleArray ||
                 texture->target == Texture::Target::TargetCubeMapArray);

            const auto isCubeMap =
                (texture->target == Texture::Target::TargetCubeMap ||
                 texture->target == Texture::Target::TargetCubeMapArray);

            setFormVisibility(mImageProperties->formLayout,
                mImageProperties->labelLayer,
                mImageProperties->layer, isArray);

            setFormVisibility(mImageProperties->formLayout,
                mImageProperties->labelFace,
                mImageProperties->face, isCubeMap);
        }
}
