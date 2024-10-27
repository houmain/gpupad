#pragma once

#include "Item.h"
#include <QVariant>
#include <QWidget>

namespace Ui {
    class AttachmentProperties;
}

class QDataWidgetMapper;
class PropertiesEditor;

class AttachmentProperties final : public QWidget
{
    Q_OBJECT
public:
    explicit AttachmentProperties(PropertiesEditor *propertiesEditor);
    ~AttachmentProperties();

    TextureKind currentTextureKind() const;
    void addMappings(QDataWidgetMapper &mapper);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void updateWidgets();

    PropertiesEditor &mPropertiesEditor;
    Ui::AttachmentProperties *mUi;
};
