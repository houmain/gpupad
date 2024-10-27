#pragma once

#include "Item.h"
#include <QVariant>
#include <QWidget>

namespace Ui {
    class CallProperties;
}

class QDataWidgetMapper;
class PropertiesEditor;

class CallProperties final : public QWidget
{
    Q_OBJECT
public:
    explicit CallProperties(PropertiesEditor *propertiesEditor);
    ~CallProperties();

    Call::CallType currentType() const;
    Call::PrimitiveType currentPrimitiveType() const;
    CallKind currentCallKind() const;
    TextureKind currentTextureKind() const;
    void addMappings(QDataWidgetMapper &mapper);

private:
    void updateWidgets();

    PropertiesEditor &mPropertiesEditor;
    Ui::CallProperties *mUi;
};
