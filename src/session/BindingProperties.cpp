#include "BindingProperties.h"
#include "SessionModel.h"
#include "SessionProperties.h"
#include "ui_BindingProperties.h"

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

    QColor valueToColor(const QStringList &values)
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

    QStringList colorToValue(QColor color)
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
        [this]() { updateValue(mUi->expressions->fields()); });

    connect(mUi->color, &ColorPicker::colorChanged,
        [this](QColor color) { updateValue(colorToValue(color)); });
    connect(mUi->reference, &ReferenceComboBox::currentDataChanged,
        [this](QVariant id) { updateValue({ id.toString() }); });
    connect(mUi->reference, &ReferenceComboBox::textRequired,
        [this](QVariant id) {
            return mSessionProperties.model().findItemName(id.toInt());
        });
    connect(mUi->reference, &ReferenceComboBox::listRequired,
        this, &BindingProperties::getItemIds);

    updateWidgets();
}

BindingProperties::~BindingProperties()
{
    delete mUi;
}

QWidget *BindingProperties::typeWidget() const { return mUi->type; }
QWidget *BindingProperties::editorWidget() const { return mUi->editor; }

Binding::Type BindingProperties::currentType() const
{
    return static_cast<Binding::Type>(mUi->type->currentData().toInt());
}

Binding::Editor BindingProperties::currentEditor() const
{
    return static_cast<Binding::Editor>(mUi->editor->currentData().toInt());
}

void BindingProperties::setValues(const QVariantList &values)
{
    if (mValues != values) {
        mValues = values;
        mUi->count->setValue(mValues.size());
        updateWidgets();
    }
}

void BindingProperties::updateValue(QStringList fields)
{
    if (mSuspendUpdateValue)
        return;

    const auto index = mUi->index->currentIndex();
    mValues[index] = fields;
    emit valuesChanged(mValues);
}

void BindingProperties::updateWidgets()
{
    mSuspendUpdateValue = true;

    const auto count = mUi->count->value();
    while (mUi->index->count() > count)
        mUi->index->removeTab(mUi->index->count() - 1);
    for (auto i = mUi->index->count(); i < mUi->count->value(); ++i)
        mUi->index->addTab(new QWidget(), QString::number(i));
    mUi->index->setVisible(count > 1);

    while (mValues.size() < count)
        mValues.push_back(QVariantList());
    while (mValues.size() > count)
        mValues.removeLast();
    auto value = mValues[mUi->index->currentIndex()].toStringList();

    const auto type = currentType();
    const auto editor = currentEditor();
    const auto isImage = (type == Binding::Image);
    const auto isReference = (type != Binding::Uniform);
    const auto isColor = (type == Binding::Uniform && editor == Binding::Color);

    if (isReference) {
        const auto id = (!value.isEmpty() ? value.first().toInt() : 0);
        mUi->reference->setCurrentData(id);
        mUi->reference->validate();
    }
    else if (isColor) {
        mUi->color->setColor(valueToColor(value));
    }
    else {
        mUi->expressions->setColumnCount(expressionColumns(editor));
        mUi->expressions->setRowCount(expressionRows(editor));
        mUi->expressions->setFields(value);
    }


    mUi->expressions->setVisible(!isReference && !isColor);
    mUi->color->setVisible(isColor);
    mUi->reference->setVisible(isReference);

    setFormVisibility(mUi->formLayout, mUi->labelEditor, mUi->editor, !isReference);
    setFormVisibility(mUi->formLayout, mUi->labelValue, mUi->value, true);
    setFormVisibility(mUi->formLayout, mUi->labelLevel, mUi->level, isImage);

    mSuspendUpdateValue = false;
}

QVariantList BindingProperties::getItemIds() const
{
    const auto bindingType = currentType();
    const auto referenceType =
        (bindingType == Binding::Texture ? ItemType::Texture :
         bindingType == Binding::Sampler ? ItemType::Sampler :
         bindingType == Binding::Image ? ItemType::Texture :
         ItemType::Buffer);

    return mSessionProperties.getItemIds(referenceType);
}
