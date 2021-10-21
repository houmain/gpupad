#include "TextureInfoBar.h"
#include "ui_TextureInfoBar.h"
#include <QVector4D>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>

class Histogram : public QWidget
{
private:
    const int height = 70;
    QVector<float> mHistogram;

public:
    Histogram(QWidget *parent)
        : QWidget(parent)
    {
        setMinimumSize(1, height + 1);
        setMaximumSize(4096, height + 1);
    }

    QSize sizeHint() const override 
    {
        return { 4096, height + 1 };
    }

    void updateHistogram(const QVector<float> &histogram)
    {
        mHistogram = histogram;
        update();
    }

    void paintEvent(QPaintEvent *ev) override
    {
        QPainter painter(this);
        painter.fillRect(ev->rect(), QBrush(QColor::fromRgb(0, 0, 0, 50)));
        painter.setPen(QPen(QColor::fromRgb(0, 0, 0, 64)));
        painter.drawRect(ev->rect().adjusted(0, 0, -1, -1));

        QPainterPath red, green, blue;
        red.moveTo(1, height);
        green.moveTo(1, height);
        blue.moveTo(1, height);
        auto i = 0;
        for (; i < mHistogram.size(); i += 3) {
            red.lineTo(i + 3 + 1, mHistogram[i] * height);
            green.lineTo(i + 3 + 1, mHistogram[i + 1] * height);
            blue.lineTo(i + 3 + 1, mHistogram[i + 2] * height);
        }
        red.lineTo(i + 3 + 1, height);
        green.lineTo(i + 3 + 1, height);
        blue.lineTo(i + 3 + 1, height);
        painter.setCompositionMode(QPainter::CompositionMode_Plus);
        painter.fillPath(blue, QBrush(QColor::fromRgb(0, 32, 255, 255)));
        painter.fillPath(red, QBrush(QColor::fromRgb(255, 0, 0, 255)));
        painter.fillPath(green, QBrush(QColor::fromRgb(0, 255, 0, 255)));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.strokePath(blue, QPen(QColor::fromRgb(0, 0, 255, 64)));
        painter.strokePath(red, QPen(QColor::fromRgb(255, 0, 0, 64)));
        painter.strokePath(green, QPen(QColor::fromRgb(0, 255, 0, 64)));
    }
};

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

void TextureInfoBar::setMappingRange(const DoubleRange &range)
{
    if (mMappingRange != range) {
        mMappingRange = range;
        Q_EMIT mappingRangeChanged(range);
    }
}

void TextureInfoBar::setHistogramBounds(const DoubleRange &bounds)
{
    if (mHistogramBounds != bounds) {
        mHistogramBounds = bounds;
        ui->minimum->setValue(bounds.minimum);
        ui->maximum->setValue(bounds.maximum);
        Q_EMIT histogramBoundsChanged(bounds);
    }
    setMappingRange(bounds);
}

void TextureInfoBar::updateHistogram(const QVector<float> &histogramUpdate)
{
    mHistogram->updateHistogram(histogramUpdate);
}

void TextureInfoBar::resetRange()
{
    setHistogramBounds({ 0, 1 });
}

int TextureInfoBar::histogramBinCount() const
{
    return mHistogram->width();
}

void TextureInfoBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    Q_EMIT histogramBinCountChanged(histogramBinCount());
}

