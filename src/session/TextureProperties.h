#ifndef TEXTUREPROPERTIES_H
#define TEXTUREPROPERTIES_H

#include "Item.h"
#include <QWidget>

namespace Ui {
class TextureProperties;
}

class SessionProperties;

class TextureProperties : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QVariant format READ format WRITE setFormat
        NOTIFY formatChanged USER true)
public:
    explicit TextureProperties(SessionProperties *sessionProperties);
    ~TextureProperties();

    QWidget *fileWidget() const;
    QWidget *targetWidget() const;
    QWidget *widthWidget() const;
    QWidget *heightWidget() const;
    QWidget *depthWidget() const;
    QWidget *flipYWidget() const;

    QVariant format() const { return static_cast<int>(mFormat); }
    void setFormat(QVariant format);

signals:
    void formatChanged();

private:
    void updateWidgets();
    void updateFormatDataWidget(QVariant formatType);
    void updateFormat(QVariant formatData);
    void updateSize();

    SessionProperties &mSessionProperties;
    Ui::TextureProperties *mUi;
    Texture::Format mFormat{ };
    bool mSuspendUpdateFormat{ };
};

#endif // TEXTUREPROPERTIES_H
