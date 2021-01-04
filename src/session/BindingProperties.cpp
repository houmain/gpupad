#include "BindingProperties.h"
#include "SessionModel.h"
#include "SessionProperties.h"
#include "TextureData.h"
#include "ui_BindingProperties.h"
#include <QDataWidgetMapper>

namespace {
    int expressionColumns(Binding::Editor editor)
    {
        switch (editor) {
            case Binding::Editor::Expression2x2:
            case Binding::Editor::Expression3x2:
            case Binding::Editor::Expression4x2: return 2;
            case Binding::Editor::Expression2x3:
            case Binding::Editor::Expression3x3:
            case Binding::Editor::Expression4x3: return 3;
            case Binding::Editor::Expression2x4:
            case Binding::Editor::Expression3x4:
            case Binding::Editor::Expression4x4: return 4;
            default: return 1;
        }
    }

    int expressionRows(Binding::Editor editor)
    {
        switch (editor) {
            case Binding::Editor::Expression2x2:
            case Binding::Editor::Expression2x3:
            case Binding::Editor::Expression2x4:
            case Binding::Editor::Expression2: return 2;
            case Binding::Editor::Expression3x2:
            case Binding::Editor::Expression3x3:
            case Binding::Editor::Expression3x4:
            case Binding::Editor::Expression3: return 3;
            case Binding::Editor::Expression4x2:
            case Binding::Editor::Expression4x3:
            case Binding::Editor::Expression4x4:
            case Binding::Editor::Expression4:
            case Binding::Editor::Color: return 4;
            default: return 1;
        }
    }

    QColor valuesToColor(const QStringList &values)
    {
        auto clamp = [](const auto &value) {
            return std::min(std::max(0.0, value.toDouble()), 1.0);
        };

        auto rgba = QColor::fromRgbF(0, 0, 0, 1);
        if (values.size() > 0) rgba.setRedF(clamp(values[0]));
        if (values.size() > 1) rgba.setGreenF(clamp(values[1]));
        if (values.size() > 2) rgba.setBlueF(clamp(values[2]));
        if (values.size() > 3) rgba.setAlphaF(clamp(values[3]));
        return rgba;
    }

    QStringList colorToValues(QColor color)
    {
        return {
            QString::number(color.redF()),
            QString::number(color.greenF()),
            QString::number(color.blueF()),
            QString::number(color.alphaF())
        };
    }

    bool startsWith(const QStringList &a, const QStringList &b)
    {
        if (b.size() > a.size())
            return false;
        for (auto i = 0; i < b.size(); ++i)
            if (a[i] != b[i])
                return false;
        return true;
    }
} // namespace

BindingProperties::BindingProperties(SessionProperties *sessionProperties)
    : QWidget(sessionProperties)
    , mSessionProperties(*sessionProperties)
    , mUi(new Ui::BindingProperties)
{
    mUi->setupUi(this);

    fillComboBox<Binding::BindingType>(mUi->type);

    fillComboBox<Binding::Editor>(mUi->editor, {
        { "Expression", Binding::Editor::Expression },
        { "2 Expressions", Binding::Editor::Expression2 },
        { "3 Expressions", Binding::Editor::Expression3 },
        { "4 Expressions", Binding::Editor::Expression4 },
        { "2x2 Expressions", Binding::Editor::Expression2x2 },
        { "2x3 Expressions", Binding::Editor::Expression2x3 },
        { "2x4 Expressions", Binding::Editor::Expression2x4 },
        { "3x2 Expressions", Binding::Editor::Expression3x2 },
        { "3x3 Expressions", Binding::Editor::Expression3x3 },
        { "3x4 Expressions", Binding::Editor::Expression3x4 },
        { "4x2 Expressions", Binding::Editor::Expression4x2 },
        { "4x3 Expressions", Binding::Editor::Expression4x3 },
        { "4x4 Expressions", Binding::Editor::Expression4x4 },
        { "Color", Binding::Editor::Color },
    });

    fillComboBox<QOpenGLTexture::Filter>(mUi->minFilter, {
        { "Nearest", QOpenGLTexture::Nearest },
        { "Linear", QOpenGLTexture::Linear },
        { "Nearest MipMap-Nearest", QOpenGLTexture::NearestMipMapNearest },
        { "Nearest MipMap-Linear", QOpenGLTexture::NearestMipMapLinear },
        { "Linear MipMap-Nearest", QOpenGLTexture::LinearMipMapNearest },
        { "Linear MipMap-Linear", QOpenGLTexture::LinearMipMapLinear },
    });

    fillComboBox<QOpenGLTexture::Filter>(mUi->magFilter, {
        { "Nearest", QOpenGLTexture::Nearest },
        { "Linear", QOpenGLTexture::Linear },
    });

    fillComboBox<QOpenGLTexture::WrapMode>(mUi->wrapModeX);
    mUi->wrapModeY->setModel(mUi->wrapModeX->model());
    mUi->wrapModeZ->setModel(mUi->wrapModeX->model());

    fillComboBox<Binding::ComparisonFunc>(mUi->comparisonFunc);

    connect(mUi->type, &DataComboBox::currentDataChanged,
        this, &BindingProperties::updateWidgets);
    connect(mUi->editor, &DataComboBox::currentDataChanged,
        this, &BindingProperties::updateWidgets);
    connect(mUi->expressions, &ExpressionMatrix::itemChanged,
        [this]() { setValues(mUi->expressions->values()); });
    connect(mUi->color, &ColorPicker::colorChanged,
        [this](QColor color) { setValues(colorToValues(color)); });

    connect(mUi->texture, &ReferenceComboBox::currentDataChanged,
        this, &BindingProperties::updateWidgets);
    connect(mUi->texture, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(Item::Type::Texture); });
    connect(mUi->buffer, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(Item::Type::Buffer); });
    connect(mUi->block, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(Item::Type::Block); });
    connect(mUi->block, &ReferenceComboBox::currentDataChanged,
        this, &BindingProperties::updateWidgets);
    for (auto comboBox : { mUi->texture, mUi->buffer, mUi->block })
        connect(comboBox, &ReferenceComboBox::textRequired,
            [this](QVariant id) {
                return mSessionProperties.getItemName(id.toInt());
            });

    // TODO: set layer to -1 when disabled...
    mUi->layered->setVisible(false);

    updateWidgets();
}

BindingProperties::~BindingProperties()
{
    delete mUi;
}

void BindingProperties::addMappings(QDataWidgetMapper &mapper)
{
    mapper.addMapping(mUi->type, SessionModel::BindingType);
    mapper.addMapping(mUi->editor, SessionModel::BindingEditor);
    mapper.addMapping(mUi->texture, SessionModel::BindingTextureId);
    mapper.addMapping(mUi->buffer, SessionModel::BindingBufferId);
    mapper.addMapping(mUi->block, SessionModel::BindingBlockId);
    mapper.addMapping(mUi->level, SessionModel::BindingLevel);
    mapper.addMapping(mUi->layer, SessionModel::BindingLayer);
    mapper.addMapping(mUi->minFilter, SessionModel::BindingMinFilter);
    mapper.addMapping(mUi->magFilter, SessionModel::BindingMagFilter);
    mapper.addMapping(mUi->anisotropic, SessionModel::BindingAnisotropic);
    mapper.addMapping(mUi->wrapModeX, SessionModel::BindingWrapModeX);
    mapper.addMapping(mUi->wrapModeY, SessionModel::BindingWrapModeY);
    mapper.addMapping(mUi->wrapModeZ, SessionModel::BindingWrapModeZ);
    mapper.addMapping(mUi->borderColor, SessionModel::BindingBorderColor);
    mapper.addMapping(mUi->comparisonFunc, SessionModel::BindingComparisonFunc);
    mapper.addMapping(mUi->subroutine, SessionModel::BindingSubroutine);
    mapper.addMapping(this, SessionModel::BindingValues);

    // populate filtered image formats before binding
    mapper.submit();
    updateWidgets();
    mapper.addMapping(mUi->imageFormat, SessionModel::BindingImageFormat);
}

Binding::BindingType BindingProperties::currentType() const
{
    return static_cast<Binding::BindingType>(mUi->type->currentData().toInt());
}

Binding::Editor BindingProperties::currentEditor() const
{
    return static_cast<Binding::Editor>(mUi->editor->currentData().toInt());
}

TextureKind BindingProperties::currentTextureKind() const
{
    auto itemId = mUi->texture->currentData().toInt();
    if (auto texture = mSessionProperties.model().findItem<Texture>(itemId))
        return getKind(*texture);
    return { };
}

int BindingProperties::getTextureStride(QVariant textureId) const
{
    const auto itemId = textureId.toInt();
    if (auto texture = mSessionProperties.model().findItem<Texture>(itemId))
        return getTextureDataSize(getTextureDataType(texture->format)) *
            getTextureComponentCount(texture->format);
    return 0;
}

int BindingProperties::getBlockStride(QVariant blockId) const
{
    const auto itemId = blockId.toInt();
    if (auto block = mSessionProperties.model().findItem<Block>(itemId))
        return ::getBlockStride(*block);
    return 0;
}

void BindingProperties::setValues(const QStringList &values)
{
    if (mSuspendSetValues ||
        mValues == values)
        return;

    // remember more values, to allow scrolling through dimensions without losing entries
    if (values.empty() || !startsWith(mValues, values))
        mValues = values;

    Q_EMIT valuesChanged();

    updateWidgets();
}

QStringList BindingProperties::values() const
{
    return mUi->expressions->values();
}

void BindingProperties::updateWidgets()
{
    const auto type = currentType();
    const auto editor = currentEditor();
    const auto uniform = (type == Binding::BindingType::Uniform);
    const auto sampler = (type == Binding::BindingType::Sampler);
    const auto image = (type == Binding::BindingType::Image);
    const auto textureBuffer = (type == Binding::BindingType::TextureBuffer);
    const auto buffer = (type == Binding::BindingType::Buffer);
    const auto color = (type == Binding::BindingType::Uniform &&
                        editor == Binding::Editor::Color);
    const auto subroutine = (type == Binding::BindingType::Subroutine);

    mSuspendSetValues = true;

    setFormVisibility(mUi->formLayout, mUi->labelEditor, mUi->editor, uniform);
    setFormVisibility(mUi->formLayout, mUi->labelExpressions, mUi->expressions,
        uniform && !color);
    setFormVisibility(mUi->formLayout, mUi->labelColor, mUi->color, color);
    mUi->expressions->setColumnCount(expressionColumns(editor));
    mUi->expressions->setRowCount(expressionRows(editor));
    mUi->color->setColor(valuesToColor(mValues));
    mUi->expressions->setValues(mValues);

    setFormVisibility(mUi->formLayout, mUi->labelTexture, mUi->texture,
        image || sampler);
    setFormVisibility(mUi->formLayout, mUi->labelBuffer, mUi->buffer,
        buffer);
    setFormVisibility(mUi->formLayout, mUi->labelBlock, mUi->block,
        textureBuffer);

    const auto textureKind = (image ? currentTextureKind() : TextureKind{ });
    setFormVisibility(mUi->formLayout, mUi->labelLevel, mUi->level, image);
    setFormVisibility(mUi->formLayout, mUi->labelLayer, mUi->layerWidget,
        textureKind.array || textureKind.dimensions == 3 || textureKind.cubeMap);

    auto stride = 0;
    if (image)
        stride = getTextureStride(mUi->texture->currentData());
    else if (textureBuffer)
        stride = getBlockStride(mUi->block->currentData());
    filterImageFormats(stride);
    setFormVisibility(mUi->formLayout, mUi->labelImageFormat, mUi->imageFormat,
        stride);

    setFormVisibility(mUi->formLayout, mUi->labelComparisonFunc, mUi->comparisonFunc,
        sampler && textureKind.depth);
    setFormVisibility(mUi->formLayout, mUi->labelMagFilter, mUi->magFilter,
        sampler);
    setFormVisibility(mUi->formLayout, mUi->labelMinFilter, mUi->minFilter,
        sampler);
    setFormVisibility(mUi->formLayout, mUi->labelAnisotropic, mUi->anisotropic,
        sampler);
    setFormVisibility(mUi->formLayout, mUi->labelWrapModeX, mUi->wrapModeX,
        sampler && textureKind.dimensions > 0);
    setFormVisibility(mUi->formLayout, mUi->labelWrapModeY, mUi->wrapModeY,
        sampler && textureKind.dimensions > 1);
    setFormVisibility(mUi->formLayout, mUi->labelWrapModeZ, mUi->wrapModeZ,
        sampler && textureKind.dimensions > 2);
    setFormVisibility(mUi->formLayout, mUi->labelBorderColor, mUi->borderColor,
        sampler);

    setFormVisibility(mUi->formLayout, mUi->labelSubroutine, mUi->subroutine,
        subroutine);

    mSuspendSetValues = false;
}

void BindingProperties::filterImageFormats(int stride)
{
    const auto type = currentType();
    const auto image = (type == Binding::BindingType::Image);
    const auto textureBuffer = (type == Binding::BindingType::TextureBuffer);
    mUi->imageFormat->clear();
    mUi->imageFormat->setEnabled(true);

    if (stride == 1) {
        fillComboBox<Binding::ImageFormat>(mUi->imageFormat, {
            { (image ? "Internal" : nullptr), Binding::ImageFormat::Internal },
            { "R 8 Bit", Binding::ImageFormat::r8 },
            { "R 8 Bit Unsigned Int", Binding::ImageFormat::r8ui },
            { "R 8 Bit Signed Int", Binding::ImageFormat::r8i },
        });
    }
    else if (stride == 2) {
        fillComboBox<Binding::ImageFormat>(mUi->imageFormat, {
            { (image ? "Internal" : nullptr), Binding::ImageFormat::Internal },
            { "R 16 Bit", Binding::ImageFormat::r16 },
            { (image ? "R 16 Bit Signed" : nullptr), Binding::ImageFormat::r16_snorm },
            { "R 16 Bit Float", Binding::ImageFormat::r16f },
            { "R 16 Bit Unsigned Int", Binding::ImageFormat::r16ui },
            { "R 16 Bit Signed  Int", Binding::ImageFormat::r16i },
            { "RG 8 Bit", Binding::ImageFormat::rg8 },
            { (image ? "RG 8 Bit Signed" : nullptr), Binding::ImageFormat::rg8_snorm },
            { "RG 8 Bit Unsigned Int", Binding::ImageFormat::rg8ui },
            { "RG 8 Bit Signed Int", Binding::ImageFormat::rg8i },
        });
    }
    else if (stride == 3 && textureBuffer) {
        fillComboBox<Binding::ImageFormat>(mUi->imageFormat, {
            { "RGB 32 Bit Float", Binding::ImageFormat::rgb32f },
            { "RGB 32 Bit Unsigned Int", Binding::ImageFormat::rgb32ui },
            { "RGB 32 Bit Signed Int", Binding::ImageFormat::rgb32i },
        });
    }
    else if (stride == 4) {
        fillComboBox<Binding::ImageFormat>(mUi->imageFormat, {
            { (image ? "Internal" : nullptr), Binding::ImageFormat::Internal },
            { "R 32 Bit Float", Binding::ImageFormat::r32f },
            { "R 32 Bit Unsigned Int", Binding::ImageFormat::r32ui },
            { "R 32 Bit Signed Int", Binding::ImageFormat::r32i },
            { "RG 16 Bit", Binding::ImageFormat::rg16 },
            { (image ? "RG 16 Bit Signed" : nullptr), Binding::ImageFormat::rg16_snorm },
            { "RG 16 Bit Unsigned Int", Binding::ImageFormat::rg16ui },
            { "RG 16 Bit Signed Int", Binding::ImageFormat::rg16i },
            { "RG 16 Bit Float", Binding::ImageFormat::rg16f },
            { "RGBA 8 Bit", Binding::ImageFormat::rgba8 },
            { (image ? "RGBA 8 Bit Signed" : nullptr), Binding::ImageFormat::rgba8_snorm },
            { "RGBA 8 Bit Unsigned Int", Binding::ImageFormat::rgba8ui },
            { "RGBA 8 Bit Signed Int", Binding::ImageFormat::rgba8i },
            { (image ? "RGBA 10/10/10/2 Bit" : nullptr), Binding::ImageFormat::rgb10_a2 },
            { (image ? "RGBA 10/10/10/2 Bit UInt" : nullptr), Binding::ImageFormat::rgb10_a2ui },
            { (image ? "RGBA 11/11/10 Bit Float" : nullptr), Binding::ImageFormat::r11f_g11f_b10f },
        });
    }
    else if (stride == 8) {
        fillComboBox<Binding::ImageFormat>(mUi->imageFormat, {
            { (image ? "Internal" : nullptr), Binding::ImageFormat::Internal },
            { "RG 32 Bit Float", Binding::ImageFormat::rg32f },
            { "RG 32 Bit Unsigned Int", Binding::ImageFormat::rg32ui },
            { "RG 32 Bit Signed Int", Binding::ImageFormat::rg32i },
            { "RGBA 16 Bit", Binding::ImageFormat::rgba16 },
            { (image ? "RGBA 16 Bit Signed" : nullptr), Binding::ImageFormat::rgba16_snorm },
            { "RGBA 16 Bit Float", Binding::ImageFormat::rgba16f },
            { "RGBA 16 Bit Unsigned Int", Binding::ImageFormat::rgba16ui },
            { "RGBA 16 Bit Signed Int", Binding::ImageFormat::rgba16i },
        });
    } else if (stride == 16) {
        fillComboBox<Binding::ImageFormat>(mUi->imageFormat, {
            { (image ? "Internal" : nullptr), Binding::ImageFormat::Internal },
            { "RGBA 32 Bit Float", Binding::ImageFormat::rgba32f },
            { "RGBA 32 Bit Unsigned Int", Binding::ImageFormat::rgba32i },
            { "RGBA 32 Bit Signed Int", Binding::ImageFormat::rgba32ui },
        });
    }
    else {
        fillComboBox<Binding::ImageFormat>(mUi->imageFormat, {
            { "Invalid stride", Binding::ImageFormat::Internal }
        });
        mUi->imageFormat->setEnabled(false);
    }
}
