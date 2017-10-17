#include "ColorPicker.h"
#include <QColorDialog>

ColorPicker::ColorPicker(QWidget *parent) : QToolButton(parent)
{
    setIconSize(QSize(16, 16));
    setAutoFillBackground(true);
    connect(this, &ColorPicker::clicked,
        this, &ColorPicker::openColorDialog);
}

void ColorPicker::setColor(QColor color)
{
    if (color != mColor) {
        mColor = color;
        setStyleSheet("QToolButton { background: " +
            color.name(QColor::HexRgb) + "}");
        emit colorChanged(color);
    }
}

void ColorPicker::openColorDialog()
{
    auto prevColor = mColor;
    QColorDialog dialog(mColor, this);
    connect(&dialog, &QColorDialog::currentColorChanged,
        this, &ColorPicker::setColor);
    dialog.setOption(QColorDialog::ShowAlphaChannel);
    dialog.setCurrentColor(mColor);
    setColor(dialog.exec() == QDialog::Accepted ?
        dialog.selectedColor() : prevColor);
}
