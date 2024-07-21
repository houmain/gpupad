#pragma once

#include <QWidget>
#include "Range.h"

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
    void updateHistogram(const QVector<qreal> &histogramUpdate);
    int histogramBinCount() const;
    void setMappingRange(const Range &range);
    const Range &mappingRange() const;
    void setHistogramBounds(const Range &bounds);
    const Range &histogramBounds() const { return mHistogramBounds; }
    void resetRange();
    void invertRange();
    void setColorMask(unsigned int colorMask);
    unsigned int colorMask() const { return mColorMask; }

    void resizeEvent(QResizeEvent *event) override;

Q_SIGNALS:
    void cancelled();
    void pickerEnabledChanged(bool enabled);
    void mappingRangeChanged(const Range &range);
    void histogramBinCountChanged(int count);
    void histogramBoundsChanged(const Range &bounds);
    void autoRangeRequested();
    void colorMaskChanged(unsigned int colorMask);

private:
    void handleColorMaskToggled();

    Ui::TextureInfoBar *ui;
    Histogram *mHistogram;
    bool mIsPickerEnabled{ };
    Range mHistogramBounds{ };
    unsigned int mColorMask{ };
};

