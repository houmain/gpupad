#ifndef COLORPICKER_H
#define COLORPICKER_H

#include <QToolButton>

class ColorPicker final : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged USER true)
public:
    explicit ColorPicker(QWidget *parent = nullptr);
    QColor color() const { return mColor; }
    void setColor(QColor color);

Q_SIGNALS:
    void colorChanged(QColor color);

private:
    void openColorDialog();

    QColor mColor{ Qt::white };
};

#endif // COLORPICKER_H
