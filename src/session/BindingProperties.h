#ifndef BINDINGPROPERTIES_H
#define BINDINGPROPERTIES_H

#include "ItemFunctions.h"
#include <QWidget>
#include <QVariant>

namespace Ui {
class BindingProperties;
}

class SessionProperties;
class QDataWidgetMapper;

class BindingProperties : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList values READ values WRITE setValues
        NOTIFY valuesChanged USER true)
public:
    explicit BindingProperties(SessionProperties *sessionProperties);
    ~BindingProperties();

    void addMappings(QDataWidgetMapper &mapper);
    Binding::BindingType currentType() const;
    Binding::Editor currentEditor() const;
    TextureKind currentTextureKind() const;
    void setValues(const QStringList &values);
    const QStringList &values() const { return mValues; }

signals:
    void valuesChanged();

private:
    void updateWidgets();

    SessionProperties &mSessionProperties;
    Ui::BindingProperties *mUi;
    QStringList mValues;
    bool mSuspendSetValues{ };
};

#endif // BINDINGPROPERTIES_H
