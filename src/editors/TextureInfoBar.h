#ifndef TEXTUREINFOBAR_H
#define TEXTUREINFOBAR_H

#include <QWidget>

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

Q_SIGNALS:
    void cancelled();
    void pickerEnabledChanged(bool enabled);

private:
    Ui::TextureInfoBar *ui;
    Histogram *mHistogram;
    bool mIsPickerEnabled{ };
};

#endif // TEXTUREINFOBAR_H
