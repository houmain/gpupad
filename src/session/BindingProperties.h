#ifndef BINDINGPROPERTIES_H
#define BINDINGPROPERTIES_H

#include "Item.h"
#include <QWidget>
#include <QVariant>

namespace Ui {
class BindingProperties;
}

class SessionProperties;

class BindingProperties : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QVariantList values READ values WRITE setValues
        NOTIFY valuesChanged USER true)
public:
    explicit BindingProperties(SessionProperties *sessionProperties);
    ~BindingProperties();

    QWidget *typeWidget() const;
    QWidget *dataTypeWidget() const;
    Binding::Type currentType() const;
    void setValues(const QVariantList &values);
    const QVariantList &values() const { return mValues; }

signals:
    void valuesChanged(const QVariantList &values);

private:
    void updateValue(QStringList fields);
    void updateWidgets();
    QVariantList getItemIds() const;

    SessionProperties &mSessionProperties;
    Ui::BindingProperties *mUi;
    QVariantList mValues;
    bool mSuspendUpdateValue{ };
};

#endif // BINDINGPROPERTIES_H
