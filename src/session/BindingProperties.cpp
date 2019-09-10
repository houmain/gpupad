#include "BindingProperties.h"
#include "SessionModel.h"
#include "SessionProperties.h"
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
    for (auto comboBox : { mUi->texture, mUi->buffer })
        connect(comboBox, &ReferenceComboBox::textRequired,
            [this](QVariant id) {
                return mSessionProperties.findItemName(id.toInt());
            });

    connect(mUi->layered, &QCheckBox::toggled,
        mUi->layer, &QWidget::setEnabled);
    mUi->layer->setEnabled(false);

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
    mapper.addMapping(mUi->level, SessionModel::BindingLevel);
    mapper.addMapping(mUi->layered, SessionModel::BindingLayered);
    mapper.addMapping(mUi->layer, SessionModel::BindingLayer);
    mapper.addMapping(mUi->minFilter, SessionModel::BindingMinFilter);
    mapper.addMapping(mUi->magFilter, SessionModel::BindingMagFilter);
    mapper.addMapping(mUi->wrapModeX, SessionModel::BindingWrapModeX);
    mapper.addMapping(mUi->wrapModeY, SessionModel::BindingWrapModeY);
    mapper.addMapping(mUi->wrapModeZ, SessionModel::BindingWrapModeZ);
    mapper.addMapping(mUi->borderColor, SessionModel::BindingBorderColor);
    mapper.addMapping(mUi->comparisonFunc, SessionModel::BindingComparisonFunc);
    mapper.addMapping(mUi->subroutine, SessionModel::BindingSubroutine);
    mapper.addMapping(this, SessionModel::BindingValues);
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

void BindingProperties::setValues(const QStringList &values)
{
    if (mSuspendSetValues ||
        mValues == values)
        return;

    // remember more values, to allow scrolling through dimensions without losing entries
    if (!startsWith(mValues, values))
        mValues = values;

    emit valuesChanged();

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
        (type == Binding::BindingType::Buffer));

    auto textureKind = currentTextureKind();
    setFormVisibility(mUi->formLayout, mUi->labelLevel, mUi->level, image);
    setFormVisibility(mUi->formLayout, mUi->labelLayer, mUi->layerWidget,
        textureKind.array);

    setFormVisibility(mUi->formLayout, mUi->labelComparisonFunc, mUi->comparisonFunc,
        sampler && textureKind.depth);
    setFormVisibility(mUi->formLayout, mUi->labelMinFilter, mUi->minFilter,
        sampler);
    setFormVisibility(mUi->formLayout, mUi->labelMagFilter, mUi->magFilter,
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
