#pragma once

#include "Item.h"
#include <QWidget>

namespace Ui {
    class TextureProperties;
}

class SessionProperties;
class QDataWidgetMapper;

class TextureProperties final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QVariant format READ format WRITE setFormat NOTIFY formatChanged
            USER true)
public:
    explicit TextureProperties(SessionProperties *sessionProperties);
    ~TextureProperties();

    QVariant format() const { return static_cast<int>(mFormat); }
    void setFormat(QVariant format);
    void addMappings(QDataWidgetMapper &mapper);
    TextureKind currentTextureKind() const;
    bool hasFile() const;

Q_SIGNALS:
    void formatChanged();

private:
    void updateWidgets();
    void updateFormatDataWidget(QVariant formatType);
    void updateFormat(QVariant formatData);
    void applyFileFormat();

    SessionProperties &mSessionProperties;
    Ui::TextureProperties *mUi;
    Texture::Format mFormat{};
    bool mSuspendUpdateFormat{};
};
