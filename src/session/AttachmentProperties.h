#pragma once

#include "Item.h"
#include <QWidget>
#include <QVariant>

namespace Ui {
class AttachmentProperties;
}

class QDataWidgetMapper;
class SessionProperties;

class AttachmentProperties final : public QWidget
{
    Q_OBJECT
public:
    explicit AttachmentProperties(SessionProperties *sessionProperties);
    ~AttachmentProperties();

    TextureKind currentTextureKind() const;
    void addMappings(QDataWidgetMapper &mapper);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void updateWidgets();

    SessionProperties &mSessionProperties;
    Ui::AttachmentProperties *mUi;
};

