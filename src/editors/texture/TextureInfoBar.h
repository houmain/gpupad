#pragma once

#include "Range.h"
#include <QWidget>

namespace Ui {
    class TextureInfoBar;
}

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
    void setMappingRange(const Range &range);
    Range mappingRange() const;
    void setColorMask(unsigned int colorMask);
    unsigned int colorMask() const { return mColorMask; }

Q_SIGNALS:
    void cancelled();
    void pickerEnabledChanged(bool enabled);
    void mappingRangeChanged(const Range &range);
    void colorMaskChanged(unsigned int colorMask);

private:
    void handleMappingRangeChanged();
    void handleColorMaskToggled();

    Ui::TextureInfoBar *ui;
    bool mIsPickerEnabled{};
    unsigned int mColorMask{};
};
