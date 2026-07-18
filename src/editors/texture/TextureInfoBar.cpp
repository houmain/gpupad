#include "TextureInfoBar.h"
#include "ui_TextureInfoBar.h"
#include <QAction>
#include <QSignalBlocker>
#include <QVector4D>
#include <cmath>

TextureInfoBar::TextureInfoBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TextureInfoBar)
{
    ui->setupUi(this);

    setMinimumSize(0, 80);
    setMaximumSize(4096, 80);

    connect(ui->buttonClose, &QPushButton::clicked, this,
        &TextureInfoBar::cancelled);
    connect(ui->minimum, &ExpressionLineEdit::textChanged, this,
        &TextureInfoBar::handleMappingMinimumChanged);
    connect(ui->maximum, &ExpressionLineEdit::textChanged, this,
        &TextureInfoBar::handleMappingMaximumChanged);
    connect(ui->rangeSlider, &RangeSlider::lowerValueChanged, this,
        &TextureInfoBar::handleMappingSelectionChanged);
    connect(ui->rangeSlider, &RangeSlider::upperValueChanged, this,
        &TextureInfoBar::handleMappingSelectionChanged);

    for (auto button : { ui->buttonColorR, ui->buttonColorG, ui->buttonColorB,
             ui->buttonColorA })
        connect(button, &QToolButton::toggled, this,
            &TextureInfoBar::handleColorMaskToggled);
}

TextureInfoBar::~TextureInfoBar()
{
    delete ui;
}

void TextureInfoBar::setMousePosition(const QPointF &mousePosition)
{
    ui->mousePositionX->setText(QStringLiteral("x: %1").arg(mousePosition.x()));
    ui->mousePositionY->setText(QStringLiteral("y: %1").arg(mousePosition.y()));
}

void TextureInfoBar::setPickerColor(const QVector4D &color)
{
    auto c = color;

    const auto toString = [](float v) { return QString::number(v, 'f', 3); };
    ui->pickerColorR->setText(toString(c.x()));
    ui->pickerColorG->setText(toString(c.y()));
    ui->pickerColorB->setText(toString(c.z()));
    ui->pickerColorA->setText(toString(c.w()));

    // output encoded value and color mapped to range
    const auto range = selectedMappingRange();
    if (range.maximum != range.minimum)
        c = (color - QVector4D(1, 1, 1, 1) * range.minimum)
            / (range.maximum - range.minimum);
    c.setW(color.w());

    const auto toStringEncoded = [](const char *channel, float v) {
        v = std::clamp(std::isnan(v) ? 0 : v, 0.0f, 1.0f) * 255;
        return QStringLiteral("%1:  %2").arg(channel).arg(
            static_cast<int>(v + 0.5f));
    };
    ui->pickerColorRE->setText(toStringEncoded("R", c.x()));
    ui->pickerColorGE->setText(toStringEncoded("G", c.y()));
    ui->pickerColorBE->setText(toStringEncoded("B", c.z()));
    ui->pickerColorAE->setText(toStringEncoded("A", c.w()));

    const auto clamp = [](float v) { return std::clamp(v, 0.0f, 1.0f); };
    ui->color->setStyleSheet("background: "
        + QColor::fromRgbF(clamp(c.x()), clamp(c.y()), clamp(c.z()),
            clamp(c.w()))
            .name(QColor::HexArgb));
}

void TextureInfoBar::setPickerEnabled(bool enabled)
{
    if (mIsPickerEnabled != enabled) {
        mIsPickerEnabled = enabled;
        Q_EMIT pickerEnabledChanged(enabled);
    }
}

void TextureInfoBar::setMappingRange(const Range &range)
{
    mMappingRange = range;
    updateMappingRangeControls();
}

Range TextureInfoBar::mappingRange() const
{
    return mMappingRange;
}

void TextureInfoBar::handleMappingMinimumChanged()
{
    auto ok = false;
    auto minimum = ui->minimum->text().toDouble(&ok);
    if (!ok)
        return;

    const auto previousSelection = mappingSelection();
    const auto previousRange = selectedMappingRange();
    mMappingRange.minimum = minimum;
    if (previousSelection.maximum != 0)
        mMappingRange.maximum = minimum
            + (previousRange.maximum - minimum) / previousSelection.maximum;

    ui->rangeSlider->SetLowerValue(0);
    updateMappingRangeControls();
    Q_EMIT mappingRangeChanged(mMappingRange);
}

void TextureInfoBar::handleMappingMaximumChanged()
{
    auto ok = false;
    auto maximum = ui->maximum->text().toDouble(&ok);
    if (!ok)
        return;

    const auto previousSelection = mappingSelection();
    const auto previousRange = selectedMappingRange();
    mMappingRange.maximum = maximum;
    if (previousSelection.minimum != 1)
        mMappingRange.minimum = maximum
            + (previousRange.minimum - maximum)
                / (1 - previousSelection.minimum);

    ui->rangeSlider->SetUpperValue(100);
    updateMappingRangeControls();
    Q_EMIT mappingRangeChanged(mMappingRange);
}

void TextureInfoBar::setMappingSelection(const Range &selection)
{
    const auto blocker = QSignalBlocker(ui->rangeSlider);
    ui->rangeSlider->SetLowerValue(static_cast<int>(selection.minimum * 100));
    ui->rangeSlider->SetUpperValue(static_cast<int>(selection.maximum * 100));
    updateMappingRangeControls();
}

Range TextureInfoBar::mappingSelection() const
{
    return {
        ui->rangeSlider->GetLowerValue() / 100.0,
        ui->rangeSlider->GetUpperValue() / 100.0,
    };
}

void TextureInfoBar::handleMappingSelectionChanged()
{
    updateMappingRangeControls();
    Q_EMIT mappingSelectionChanged(mappingSelection());
}

void TextureInfoBar::updateMappingRangeControls()
{
    const auto digits = 1000.0;
    const auto range = selectedMappingRange();
    const auto minimumBlocker = QSignalBlocker(ui->minimum);
    const auto maximumBlocker = QSignalBlocker(ui->maximum);
    ui->minimum->setValue(std::round(range.minimum * digits) / digits);
    ui->maximum->setValue(std::round(range.maximum * digits) / digits);
}

Range TextureInfoBar::selectedMappingRange() const
{
    const auto selection = mappingSelection();
    return {
        mMappingRange.minimum + mMappingRange.range() * selection.minimum,
        mMappingRange.minimum + mMappingRange.range() * selection.maximum,
    };
}

void TextureInfoBar::handleColorMaskToggled()
{
    auto colorMask = 0u;
    colorMask |= (ui->buttonColorR->isChecked() ? 1 : 0);
    colorMask |= (ui->buttonColorG->isChecked() ? 2 : 0);
    colorMask |= (ui->buttonColorB->isChecked() ? 4 : 0);
    colorMask |= (ui->buttonColorA->isChecked() ? 8 : 0);
    setColorMask(colorMask);
}

void TextureInfoBar::setColorMask(unsigned int colorMask)
{
    if (mColorMask != colorMask) {
        mColorMask = colorMask;
        ui->buttonColorR->setChecked(colorMask & 1);
        ui->buttonColorG->setChecked(colorMask & 2);
        ui->buttonColorB->setChecked(colorMask & 4);
        ui->buttonColorA->setChecked(colorMask & 8);
        Q_EMIT colorMaskChanged(mColorMask);
    }
}
