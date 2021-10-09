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
        setMinimumSize((256 + 1) * 3 + 2, height + 1);
        setMaximumSize((256 + 1) * 3 + 2, height + 1);
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
    ui->horizontalLayout->insertWidget(3, mHistogram);

    setMinimumSize(0, 80);
    setMaximumSize(16777215, 80);

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
        this, &TextureInfoBar::autoRange);
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
    const auto map = [](auto v) { return static_cast<int>(qRound(v * 255)); };
    ui->pickerColorR->setText(QStringLiteral("R: %1").arg(map(color.x())));
    ui->pickerColorG->setText(QStringLiteral("G: %1").arg(map(color.y())));
    ui->pickerColorB->setText(QStringLiteral("B: %1").arg(map(color.z())));
    ui->pickerColorA->setText(QStringLiteral("A: %1").arg(map(color.w())));

    ui->color->setStyleSheet("background: " +
        QColor::fromRgbF(color.x(), color.y(), color.z(), color.w()).name(QColor::HexRgb));
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

void TextureInfoBar::autoRange()
{
    // TODO:
    setHistogramBounds({ 0.3, 0.8 });
}
