#include "TextureInfoBar.h"
#include "Histogram.h"
#include "ui_TextureInfoBar.h"
#include <QVector4D>
#include <cmath>

TextureInfoBar::TextureInfoBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TextureInfoBar)
    , mHistogram(new Histogram(this))
{
    ui->setupUi(this);
    ui->minimum->setDecimal(true);
    ui->maximum->setDecimal(true);
    ui->horizontalLayout->insertWidget(4, mHistogram);
    
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
    c = (color - QVector4D(1, 1, 1, 1) * mMappingRange.minimum) /
        (mMappingRange.maximum - mMappingRange.minimum);
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
    if (mMappingRange != range) {
        mMappingRange = range;
        Q_EMIT mappingRangeChanged(range);
    }
}

void TextureInfoBar::setHistogramBounds(const Range &bounds)
{
    if (mHistogramBounds != bounds) {
        mHistogramBounds = bounds;
        ui->minimum->setValue(bounds.minimum);
        ui->maximum->setValue(bounds.maximum);
        Q_EMIT histogramBoundsChanged(bounds);
    }
    setMappingRange(bounds);
}

void TextureInfoBar::updateHistogram(const QVector<qreal> &histogramUpdate)
{
    mHistogram->updateHistogram(histogramUpdate);
}

void TextureInfoBar::resetRange()
{
    setHistogramBounds({ 0, 1 });
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

