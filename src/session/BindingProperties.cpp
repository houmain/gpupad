#include "BindingProperties.h"
#include "SessionModel.h"
#include "SessionProperties.h"
#include "ui_BindingProperties.h"
#include <QDataWidgetMapper>

namespace {
    int expressionColumns(Binding::Editor editor)
    {
        switch (editor) {
            case Binding::Expression2x2:
            case Binding::Expression3x2:
            case Binding::Expression4x2: return 2;
            case Binding::Expression2x3:
            case Binding::Expression3x3:
            case Binding::Expression4x3: return 3;
            case Binding::Expression2x4:
            case Binding::Expression3x4:
            case Binding::Expression4x4: return 4;
            default: return 1;
        }
    }

    int expressionRows(Binding::Editor editor)
    {
        switch (editor) {
            case Binding::Expression2x2:
            case Binding::Expression2x3:
            case Binding::Expression2x4:
            case Binding::Expression2: return 2;
            case Binding::Expression3x2:
            case Binding::Expression3x3:
            case Binding::Expression3x4:
            case Binding::Expression3: return 3;
            case Binding::Expression4x2:
            case Binding::Expression4x3:
            case Binding::Expression4x4:
            case Binding::Expression4: return 4;
            default: return 1;
        }
    }

    QColor fieldsToColor(const QStringList &fields)
    {
        auto clamp = [](const auto &value) {
            return std::min(std::max(0.0, value.toDouble()), 1.0);
        };

        auto rgba = QColor::fromRgbF(0, 0, 0, 1);
        if (fields.size() > 0) rgba.setRedF(clamp(fields[0]));
        if (fields.size() > 1) rgba.setGreenF(clamp(fields[1]));
        if (fields.size() > 2) rgba.setBlueF(clamp(fields[2]));
        if (fields.size() > 3) rgba.setAlphaF(clamp(fields[3]));
        return rgba;
    }

    QStringList colorToFields(QColor color)
    {
        return {
            QString::number(color.redF()),
            QString::number(color.greenF()),
            QString::number(color.blueF()),
            QString::number(color.alphaF())
        };
    }
} // namespace

BindingProperties::BindingProperties(SessionProperties *sessionProperties)
    : QWidget(sessionProperties)
    , mSessionProperties(*sessionProperties)
    , mUi(new Ui::BindingProperties)
{
    mUi->setupUi(this);

    fill<Binding::Type>(mUi->type, {
        { "Uniform", Binding::Uniform },
        { "Sampler", Binding::Sampler },
        { "Image", Binding::Image },
        { "Buffer", Binding::Buffer },
        //{ "Subroutine", Binding::Subroutine },
    });

    fill<Binding::Editor>(mUi->editor, {
        { "Expression", Binding::Expression },
        { "2 Expressions", Binding::Expression2 },
        { "3 Expressions", Binding::Expression3 },
        { "4 Expressions", Binding::Expression4 },
        { "2x2 Expressions", Binding::Expression2x2 },
        { "2x3 Expressions", Binding::Expression2x3 },
        { "2x4 Expressions", Binding::Expression2x4 },
        { "3x2 Expressions", Binding::Expression3x2 },
        { "3x3 Expressions", Binding::Expression3x3 },
        { "3x4 Expressions", Binding::Expression3x4 },
        { "4x2 Expressions", Binding::Expression4x2 },
        { "4x3 Expressions", Binding::Expression4x3 },
        { "4x4 Expressions", Binding::Expression4x4 },
        { "Color", Binding::Color },
    });

    connect(mUi->type, &DataComboBox::currentDataChanged,
        this, &BindingProperties::updateWidgets);
    connect(mUi->editor, &DataComboBox::currentDataChanged,
        this, &BindingProperties::updateWidgets);
    using Overload = void(QSpinBox::*)(int);
    connect(mUi->count, static_cast<Overload>(&QSpinBox::valueChanged),
        this, &BindingProperties::updateWidgets);
    connect(mUi->index, &QTabWidget::currentChanged,
        this, &BindingProperties::updateWidgets);
    connect(mUi->expressions, &ExpressionMatrix::itemChanged,
        [this]() { setFields(mUi->expressions->fields()); });
    connect(mUi->color, &ColorPicker::colorChanged,
        [this](QColor color) { setFields(colorToFields(color)); });

    connect(mUi->item, &ReferenceComboBox::listRequired,
        this, &BindingProperties::getItemIds);
    connect(mUi->item, &ReferenceComboBox::textRequired,
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
    mapper.addMapping(mUi->count, SessionModel::BindingValueCount);
    mapper.addMapping(mUi->index, SessionModel::BindingCurrentValue, "currentIndex");
    mapper.addMapping(mUi->item, SessionModel::BindingValueItemId);
    mapper.addMapping(mUi->level, SessionModel::BindingValueLevel);
    mapper.addMapping(mUi->layered, SessionModel::BindingValueLayered);
    mapper.addMapping(mUi->layer, SessionModel::BindingValueLayer);
    mapper.addMapping(this, SessionModel::BindingValueFields);
}

Binding::Type BindingProperties::currentType() const
{
    return static_cast<Binding::Type>(mUi->type->currentData().toInt());
}

Binding::Editor BindingProperties::currentEditor() const
{
    return static_cast<Binding::Editor>(mUi->editor->currentData().toInt());
}

void BindingProperties::setFields(const QStringList &fields)
{
    if (mSuspendSetFields ||
        mFields == fields)
        return;

    mFields = fields;
    emit fieldsChanged();

    updateWidgets();
}

void BindingProperties::updateWidgets()
{
    const auto type = currentType();
    const auto editor = currentEditor();
    const auto isImage = (type == Binding::Image);
    const auto isReference = (type != Binding::Uniform);
    const auto isColor = (type == Binding::Uniform && editor == Binding::Color);

    mSuspendSetFields = true;

    const auto count = mUi->count->value();
    while (mUi->index->count() > count)
        mUi->index->removeTab(mUi->index->count() - 1);
    for (auto i = mUi->index->count(); i < mUi->count->value(); ++i)
        mUi->index->addTab(new QWidget(), QString::number(i));
    mUi->index->setVisible(count > 1);

    setFormVisibility(mUi->formLayout, mUi->labelEditor, mUi->editor, !isReference);
    setFormVisibility(mUi->formLayout, mUi->labelCount, mUi->widgetCount, true);
    setFormVisibility(mUi->formLayout, mUi->labelExpressions, mUi->expressions,
        !isReference && !isColor);

    setFormVisibility(mUi->formLayout, mUi->labelColor, mUi->color, isColor);
    mUi->expressions->setColumnCount(expressionColumns(editor));
    mUi->expressions->setRowCount(expressionRows(editor));
    mUi->color->setColor(fieldsToColor(fields()));
    mUi->expressions->setFields(fields());

    if (type != Binding::Image)
        mUi->labelTexture->setVisible(false);
    if (type != Binding::Sampler)
        mUi->labelSampler->setVisible(false);
    if (type != Binding::Buffer)
        mUi->labelBuffer->setVisible(false);

    setFormVisibility(mUi->formLayout,
        type == Binding::Image ? mUi->labelTexture :
        type == Binding::Sampler ? mUi->labelSampler : mUi->labelBuffer,
        mUi->item, isReference);
    if (isReference)
        mUi->item->validate();

    setFormVisibility(mUi->formLayout, mUi->labelLevel, mUi->level, isImage);
    setFormVisibility(mUi->formLayout, mUi->labelLayer, mUi->layerWidget, isImage);

    mSuspendSetFields = false;
}

QVariantList BindingProperties::getItemIds() const
{
    const auto bindingType = currentType();
    const auto referenceType =
        (bindingType == Binding::Sampler ? ItemType::Sampler :
         bindingType == Binding::Image ? ItemType::Texture :
         ItemType::Buffer);

    return mSessionProperties.getItemIds(referenceType);
}
