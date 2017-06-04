#ifndef COLORPICKER_H
#define COLORPICKER_H

#include <QToolButton>

class ColorPicker : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged USER true)
public:
    explicit ColorPicker(QWidget *parent = 0);
    QColor color() const { return mColor; }
    void setColor(QColor color);

signals:
    void colorChanged(QColor color);

private:
    void openColorDialog();

    QColor mColor{ Qt::white };
};

#endif // COLORPICKER_H
