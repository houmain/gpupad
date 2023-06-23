
#include "Histogram.h"
#include "getEventPosition.h"
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>

Histogram::Histogram(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::SizeHorCursor);
}

QSize Histogram::sizeHint() const
{
    return { 4096, height() };
}

void Histogram::updateHistogram(const QVector<qreal> &histogram)
{
    mHistogram = histogram;
    update();
}

void Histogram::paintEvent(QPaintEvent *ev)
{
    const auto x = 2;
    const auto y = 2;
    const auto height = this->height() - 5;
    const auto width = this->width() - 5;

    const auto makeRelative = [&](auto pos) {
        return std::clamp((pos - mHistogramBounds.minimum) /
          (mHistogramBounds.maximum - mHistogramBounds.minimum), 
          0.0, 1.0);
    };
    auto minimum = makeRelative(mMappingRange.minimum);
    auto maximum = makeRelative(mMappingRange.maximum);
    if (minimum > maximum)
        std::swap(minimum, maximum);

    QPainter painter(this);
    auto fill = ev->rect();
    painter.fillRect(fill, QBrush(QColor::fromRgb(0, 0, 0, 15)));
    fill.adjust(static_cast<int>(minimum * width) + 1, 0,
                static_cast<int>((maximum - 1) * width) - 1, 0);
    painter.fillRect(fill, QBrush(QColor::fromRgb(0, 0, 0, 35)));
    painter.setPen(QPen(QColor::fromRgb(0, 0, 0, 64)));
    painter.drawRect(ev->rect().adjusted(0, 0, -1, -1));

    QPainterPath red, green, blue;
    red.moveTo(x, y + height);
    green.moveTo(x, y + height);
    blue.moveTo(x, y + height);    
    auto px = 0.0;
    const auto step = width / (mHistogram.size() / 3.0); 
    for (auto i = 0, lx = x; i < mHistogram.size(); i += 3) {
        red.lineTo(lx, y + mHistogram[i] * height);
        green.lineTo(lx, y + mHistogram[i + 1] * height);
        blue.lineTo(lx, y + mHistogram[i + 2] * height);
        px += step;
        const auto rx = static_cast<int>(x + px);
        red.lineTo(rx, y + mHistogram[i] * height);
        green.lineTo(rx, y + mHistogram[i + 1] * height);
        blue.lineTo(rx, y + mHistogram[i + 2] * height);
        lx = rx;
    }
    red.lineTo(x + width, y + height);
    green.lineTo(x + width, y + height);
    blue.lineTo(x + width, y + height);

    painter.setCompositionMode(QPainter::CompositionMode_Plus);
    painter.fillPath(blue, QBrush(QColor::fromRgb(0, 32, 255, 255)));
    painter.fillPath(red, QBrush(QColor::fromRgb(255, 0, 0, 255)));
    painter.fillPath(green, QBrush(QColor::fromRgb(0, 255, 0, 255)));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.strokePath(green, QPen(QColor::fromRgb(0, 255, 0, 96)));
    painter.strokePath(red, QPen(QColor::fromRgb(255, 0, 0, 96)));
    painter.strokePath(blue, QPen(QColor::fromRgb(0, 0, 255, 96)));
}

void Histogram::mousePressEvent(QMouseEvent *event)
{
    const auto pos = getEventPosition(event);
    if (event->buttons() & Qt::LeftButton)
        updateRange(pos.x());
    QWidget::mousePressEvent(event);
}

void Histogram::mouseMoveEvent(QMouseEvent *event)
{
    const auto pos = getEventPosition(event);
    if (event->buttons() & Qt::LeftButton)
        updateRange(pos.x());
    QWidget::mouseMoveEvent(event);
}

void Histogram::setHistogramBounds(const Range &bounds)
{
    mHistogramBounds = bounds;
}

void Histogram::setMappingRange(const Range &range)
{
    if (std::exchange(mMappingRange, range) != range) {
        Q_EMIT mappingRangeChanged(mMappingRange);
        update();
    }
}

void Histogram::updateRange(int mouseX) 
{
    const auto x = 2;
    const auto width = this->width() - 5;
    const auto pos = std::clamp((mouseX - x) / static_cast<double>(width), 0.0, 1.0) *
        (mHistogramBounds.maximum - mHistogramBounds.minimum) + mHistogramBounds.minimum;
    
    auto range = mMappingRange;
    (std::abs(range.minimum - pos) < std::abs(range.maximum - pos) ? 
        range.minimum : range.maximum) = pos;

    setMappingRange(range);
}

