#include "TextureInfoBar.h"
#include "ui_TextureInfoBar.h"
#include <QVector4D>
#include <QPainter>
#include <QPaintEvent>

class Histogram : public QWidget
{
private:
    const int height = 50;
    QVector<quint32> mPrevHistogramUpdate;
    QVector<quint8> mHistogram;

public:
    Histogram(QWidget *parent)
        : QWidget(parent)
    {
        setMinimumSize((256 + 1) * 3 + 2, height + 1);
        setMaximumSize((256 + 1) * 3 + 2, height + 1);
    }

    void updateHistogram(const QVector<quint32> &histogramUpdate) 
    {
        mPrevHistogramUpdate.resize(histogramUpdate.size());
        mHistogram.resize(histogramUpdate.size());

        auto maxValue = quint32{ 1 };
        for (auto i = 0; i < histogramUpdate.size(); ++i) {
            const auto value = (histogramUpdate[i] - mPrevHistogramUpdate[i]);
            maxValue = qMax(maxValue, value);
        }
        auto s = 1.0f / maxValue;
        for (auto i = 0; i < histogramUpdate.size(); ++i) {
            const auto value = (histogramUpdate[i] - mPrevHistogramUpdate[i]);
            mHistogram[i] = static_cast<quint8>((1.0f - (value * s)) * height);
        }

        std::memcpy(mPrevHistogramUpdate.data(), 
            histogramUpdate.constData(), 
            histogramUpdate.size() * sizeof(quint32));

        update();
    }

    void paintEvent(QPaintEvent *ev) override
    {
        QPainter painter(this);
        painter.fillRect(ev->rect(), QBrush(QColor::fromRgb(0, 0, 0, 220)));
        painter.setPen(QPen(Qt::black));
        painter.drawRect(ev->rect().adjusted(0, 0, -1, -1));

        QPainterPath red, green, blue;
        red.moveTo(1, height);
        green.moveTo(1, height);
        blue.moveTo(1, height);
        auto i = 0;
        for (; i < mHistogram.size(); i += 3) {
            red.lineTo(i + 3 + 1, mHistogram[i]);
            green.lineTo(i + 3 + 1, mHistogram[i + 1]);
            blue.lineTo(i + 3 + 1, mHistogram[i + 2]);
        }
        red.lineTo(i + 3 + 1, height);
        green.lineTo(i + 3 + 1, height);
        blue.lineTo(i + 3 + 1, height);
        painter.setCompositionMode(QPainter::CompositionMode_Plus);
        painter.fillPath(blue, QBrush(QColor::fromRgb(0, 0, 255, 255)));
        painter.fillPath(red, QBrush(QColor::fromRgb(255, 0, 0, 255)));
        painter.fillPath(green, QBrush(QColor::fromRgb(0, 255, 0, 255)));
    }
};

TextureInfoBar::TextureInfoBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TextureInfoBar)
    , mHistogram(new Histogram(this))
{
    ui->setupUi(this);
    ui->horizontalLayout->insertWidget(3, mHistogram);

    setMinimumSize(0, 60);
    setMaximumSize(std::numeric_limits<int>::max(), 60);

    connect(ui->closeButton, &QPushButton::clicked, this,
        &TextureInfoBar::cancelled);
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

void TextureInfoBar::updateHistogram(const QVector<quint32> &histogramUpdate) 
{
    mHistogram->updateHistogram(histogramUpdate);
}
