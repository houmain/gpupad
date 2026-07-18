#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>

class RangeSlider : public QWidget
{
    Q_OBJECT
    Q_ENUMS(RangeSliderTypes)

public:
    enum Option {
        NoHandle = 0x0,
        LeftHandle = 0x1,
        RightHandle = 0x2,
        DoubleHandles = LeftHandle | RightHandle
    };
    Q_DECLARE_FLAGS(Options, Option)

    RangeSlider( QWidget* aParent = Q_NULLPTR);
    RangeSlider( Qt::Orientation ori, Options t = DoubleHandles, QWidget* aParent = Q_NULLPTR);

    void setOrientation(Qt::Orientation ori);
    QSize minimumSizeHint() const override;

    int GetMinimun() const;
    void SetMinimum(int aMinimum);

    int GetMaximun() const;
    void SetMaximum(int aMaximum);

    int GetLowerValue() const;
    void SetLowerValue(int aLowerValue);

    int GetUpperValue() const;
    void SetUpperValue(int aUpperValue);

    void SetRange(int aMinimum, int aMaximum);

protected:
    void paintEvent(QPaintEvent* aEvent) override;
    void mousePressEvent(QMouseEvent* aEvent) override;
    void mouseMoveEvent(QMouseEvent* aEvent) override;
    void mouseReleaseEvent(QMouseEvent* aEvent) override;
    void changeEvent(QEvent* aEvent) override;

    QRectF firstHandleRect() const;
    QRectF secondHandleRect() const;
    QRectF handleRect(int aValue) const;

Q_SIGNALS:
    void lowerValueChanged(int aLowerValue);
    void upperValueChanged(int aUpperValue);
    void rangeChanged(int aMin, int aMax);

public Q_SLOTS:
    void setLowerValue(int aLowerValue);
    void setUpperValue(int aUpperValue);
    void setMinimum(int aMinimum);
    void setMaximum(int aMaximum);

private:
    Q_DISABLE_COPY(RangeSlider)
    float currentPercentage();
    int validLength() const;

    int mMinimum;
    int mMaximum;
    int mLowerValue;
    int mUpperValue;
    bool mFirstHandlePressed;
    bool mSecondHandlePressed;
    int mInterval;
    int mDelta;
    QColor mBackgroudColorEnabled;
    QColor mBackgroudColorDisabled;
    QColor mBackgroudColor;
    Qt::Orientation orientation;
    Options type;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(RangeSlider::Options)
