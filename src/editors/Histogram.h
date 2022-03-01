#pragma once

#include <QWidget>
#include "Range.h"

class Histogram : public QWidget
{
    Q_OBJECT
public:
    explicit Histogram(QWidget *parent = nullptr);

    QSize sizeHint() const override;

    void updateHistogram(const QVector<qreal> &histogram);
    void setHistogramBounds(const Range &bounds);
    void setMappingRange(const Range &range);
    const Range &mappingRange() const { return mMappingRange; }

Q_SIGNALS:
    void mappingRangeChanged(Range range);

protected:
    void paintEvent(QPaintEvent *ev) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void updateRange(int mouseX);

    int mHeight{ };
    QVector<qreal> mHistogram;
    Range mHistogramBounds{ };
    Range mMappingRange{ };
};
