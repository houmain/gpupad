#include "TextureInfoBar.h"
#include "Histogram.h"
#include "ui_TextureInfoBar.h"
#include <QVector4D>
#include <QAction>
#include <cmath>

TextureInfoBar::TextureInfoBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TextureInfoBar)
    , mHistogram(new Histogram(this))
{
    ui->setupUi(this);
    ui->minimum->setDecimal(true);
    ui->maximum->setDecimal(true);
    ui->horizontalLayout->insertWidget(5, mHistogram);
    
    setMinimumSize(0, 80);
    setMaximumSize(4096, 80);
    mHistogram->setFixedHeight(70);

    connect(ui->minimum, &ExpressionLineEdit::textChanged,
        [this](const QString &text) {
            auto ok = false;
            if (auto value = text.toDouble(&ok); ok)
                setHistogramBounds({ value, mHistogramBounds.maximum });
        });
    connect(ui->maximum, &ExpressionLineEdit::textChanged,
        [this](const QString &text) {
            auto ok = false;
            if (auto value = text.toDouble(&ok); ok)
                setHistogramBounds({ mHistogramBounds.minimum, value });
        });

    connect(ui->buttonClose, &QPushButton::clicked, this,
        &TextureInfoBar::cancelled);
    connect(ui->buttonAutoRange, &QPushButton::clicked, 
        this, &TextureInfoBar::autoRangeRequested);
    connect(ui->buttonResetRange, &QPushButton::clicked, 
        this, &TextureInfoBar::resetRange);
    connect(ui->buttonInvertRange, &QPushButton::clicked, 
        this, &TextureInfoBar::invertRange);
    connect(mHistogram, &Histogram::mappingRangeChanged, 
        this, &TextureInfoBar::mappingRangeChanged);

    for (auto button : { ui->buttonColorR, ui->buttonColorG, 
                         ui->buttonColorB, ui->buttonColorA })
        connect(button, &QToolButton::toggled, 
            this, &TextureInfoBar::handleColorMaskToggled);
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

    const auto toString = [](float v) { 
        return QString::number(v, 'f', 3);
    };
    ui->pickerColorR->setText(toString(c.x()));
    ui->pickerColorG->setText(toString(c.y()));
    ui->pickerColorB->setText(toString(c.z()));
    ui->pickerColorA->setText(toString(c.w()));

    // output encoded value and color mapped to range
    const auto range = mappingRange();
    c = (color - QVector4D(1, 1, 1, 1) * range.minimum) /
        (range.maximum - range.minimum);
    c.setW(color.w());

    const auto toStringEncoded = [](const char* channel, float v) { 
        v = std::clamp(std::isnan(v) ? 0 : v, 0.0f, 1.0f) * 255;
        return QStringLiteral("%1:  %2")
            .arg(channel).arg(static_cast<int>(v + 0.5f));
    };
    ui->pickerColorRE->setText(toStringEncoded("R", c.x()));
    ui->pickerColorGE->setText(toStringEncoded("G", c.y()));
    ui->pickerColorBE->setText(toStringEncoded("B", c.z()));
    ui->pickerColorAE->setText(toStringEncoded("A", c.w()));

    const auto clamp = [](float v) { return std::clamp(v, 0.0f, 1.0f); };
    ui->color->setStyleSheet("background: " + QColor::fromRgbF(
        clamp(c.x()), clamp(c.y()), clamp(c.z()), clamp(c.w())).name(QColor::HexArgb));
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
    mHistogram->setMappingRange(range);
}

const Range &TextureInfoBar::mappingRange() const
{
    return mHistogram->mappingRange();
}

void TextureInfoBar::setHistogramBounds(const Range &bounds)
{
    if (mHistogramBounds != bounds) {
        mHistogramBounds = bounds;
        ui->minimum->setValue(bounds.minimum);
        ui->maximum->setValue(bounds.maximum);
        Q_EMIT histogramBoundsChanged(bounds);
    }
    mHistogram->setHistogramBounds(bounds);
}

void TextureInfoBar::updateHistogram(const QVector<qreal> &histogramUpdate)
{
    mHistogram->updateHistogram(histogramUpdate);
}

void TextureInfoBar::resetRange()
{
    setHistogramBounds({ 0, 1 });
    setMappingRange({ 0, 1 });
}

void TextureInfoBar::invertRange()
{
    const auto bounds = histogramBounds();
    const auto range = mappingRange();
    setHistogramBounds({ bounds.maximum, bounds.minimum });
    setMappingRange({ range.maximum, range.minimum });
}

int TextureInfoBar::histogramBinCount() const
{
    const auto minWidth = 2;
    const auto multiple = 3 * 128;
    return qMax(mHistogram->width() / multiple / minWidth, 1) * multiple;
}

void TextureInfoBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    Q_EMIT histogramBinCountChanged(histogramBinCount());
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

