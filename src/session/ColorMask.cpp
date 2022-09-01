#include "ColorMask.h"
#include <QCheckBox>
#include <QHBoxLayout>

ColorMask::ColorMask(QWidget *parent) : QWidget(parent)
{
    auto layout = new QHBoxLayout(this);
    mColorR = new QCheckBox(this);
    mColorG = new QCheckBox(this);
    mColorB = new QCheckBox(this);
    mColorA = new QCheckBox(this);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->addWidget(mColorR);
    layout->addWidget(mColorG);
    layout->addWidget(mColorB);
    layout->addWidget(mColorA);
    layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));

    for (auto color : { mColorR, mColorG, mColorB, mColorA })
        connect(color, &QCheckBox::toggled, this, &ColorMask::handleColorMaskToggled);
}

void ColorMask::setColorMask(unsigned int colorMask) 
{
    if (mColorMask != colorMask) {
        mColorMask = colorMask;
        mColorR->setChecked(colorMask & 1);
        mColorG->setChecked(colorMask & 2);
        mColorB->setChecked(colorMask & 4);
        mColorA->setChecked(colorMask & 8);
        Q_EMIT colorMaskChanged(mColorMask);
    }
}

unsigned int ColorMask::colorMask() const 
{
    return mColorMask;
}

void ColorMask::handleColorMaskToggled() 
{
    auto colorMask = unsigned int{ };
    colorMask |= (mColorR->isChecked() ? 1 : 0);
    colorMask |= (mColorG->isChecked() ? 2 : 0);
    colorMask |= (mColorB->isChecked() ? 4 : 0);
    colorMask |= (mColorA->isChecked() ? 8 : 0);
    setColorMask(colorMask);
}
