#ifndef TEXTUREINFOBAR_H
#define TEXTUREINFOBAR_H

#include <QWidget>
#include "DoubleRange.h"

namespace Ui {
class TextureInfoBar;
}

class Histogram;

class TextureInfoBar : public QWidget
{
    Q_OBJECT

public:
    explicit TextureInfoBar(QWidget *parent = nullptr);
    ~TextureInfoBar();

    void setMousePosition(const QPointF &mousePosition);
    void setPickerColor(const QVector4D &color);
    void setPickerEnabled(bool enabled);
    bool isPickerEnabled() const { return mIsPickerEnabled; }
    void updateHistogram(const QVector<float> &histogramUpdate);
    void setMappingRange(const DoubleRange &range);
    const DoubleRange &mappingRange() const { return mMappingRange; }
    void setHistogramBounds(const DoubleRange &bounds);
    const DoubleRange &histogramBounds() const { return mHistogramBounds; }
    void resetRange();
    void autoRange();

Q_SIGNALS:
    void cancelled();
    void pickerEnabledChanged(bool enabled);
    void mappingRangeChanged(const DoubleRange &range);
    void histogramBoundsChanged(const DoubleRange &bounds);

private:
    Ui::TextureInfoBar *ui;
    Histogram *mHistogram;
    bool mIsPickerEnabled{ };
    DoubleRange mMappingRange{ };
    DoubleRange mHistogramBounds{ };
};

#endif // TEXTUREINFOBAR_H
