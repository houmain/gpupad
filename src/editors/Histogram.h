#pragma once

#include <QWidget>

class Histogram : public QWidget
{
    Q_OBJECT
public:
    explicit Histogram(QWidget *parent = nullptr);

    void updateHistogram(const QVector<qreal> &histogram);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *ev) override;

private:
    int mHeight{ };
    QVector<qreal> mHistogram;
};
