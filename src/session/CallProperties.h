#pragma once

#include "Item.h"
#include <QVariant>
#include <QWidget>

namespace Ui {
    class CallProperties;
}

class QDataWidgetMapper;
class SessionProperties;

class CallProperties final : public QWidget
{
    Q_OBJECT
public:
    explicit CallProperties(SessionProperties *sessionProperties);
    ~CallProperties();

    Call::CallType currentType() const;
    Call::PrimitiveType currentPrimitiveType() const;
    CallKind currentCallKind() const;
    TextureKind currentTextureKind() const;
    void addMappings(QDataWidgetMapper &mapper);

private:
    void updateWidgets();

    SessionProperties &mSessionProperties;
    Ui::CallProperties *mUi;
};
