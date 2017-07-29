#ifndef CALLPROPERTIES_H
#define CALLPROPERTIES_H

#include "ItemFunctions.h"
#include <QWidget>
#include <QVariant>

namespace Ui {
class CallProperties;
}

class QDataWidgetMapper;
class SessionProperties;

class CallProperties : public QWidget
{
    Q_OBJECT
public:
    explicit CallProperties(SessionProperties *sessionProperties);
    ~CallProperties();

    Call::Type currentType() const;
    Call::PrimitiveType currentPrimitiveType() const;
    CallKind currentCallKind() const;
    TextureKind currentTextureKind() const;
    void addMappings(QDataWidgetMapper &mapper);

private:
    void updateWidgets();

    SessionProperties &mSessionProperties;
    Ui::CallProperties *mUi;
};

#endif // CALLPROPERTIES_H
