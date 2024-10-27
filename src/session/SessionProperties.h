#pragma once

#include "Item.h"
#include <QVariant>
#include <QWidget>

namespace Ui {
    class SessionProperties;
}

class QDataWidgetMapper;
class PropertiesEditor;

class SessionProperties final : public QWidget
{
    Q_OBJECT
public:
    explicit SessionProperties(PropertiesEditor *propertiesEditor);
    ~SessionProperties();

    void addMappings(QDataWidgetMapper &mapper);

private:
    void updateWidgets();

    PropertiesEditor &mPropertiesEditor;
    Ui::SessionProperties *mUi;
};
