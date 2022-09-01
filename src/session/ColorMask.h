#ifndef COLORMASK_H
#define COLORMASK_H

#include <QWidget>

class QCheckBox;

class ColorMask final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(unsigned int colorMask READ colorMask WRITE setColorMask NOTIFY colorMaskChanged USER true)
public:
    explicit ColorMask(QWidget *parent = nullptr);
    unsigned int colorMask() const;
    void setColorMask(unsigned int colorMask);

Q_SIGNALS:
    void colorMaskChanged(unsigned int colorMask);

private:
    void handleColorMaskToggled();

    QCheckBox *mColorR{ };
    QCheckBox *mColorG{ };
    QCheckBox *mColorB{ };
    QCheckBox *mColorA{ };
    unsigned int mColorMask{ };
};

#endif // COLORMASK_H
