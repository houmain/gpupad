#ifndef BINDINGPROPERTIES_H
#define BINDINGPROPERTIES_H

#include "Item.h"
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
    Q_PROPERTY(QStringList fields READ fields WRITE setFields
        NOTIFY fieldsChanged USER true)
public:
    explicit BindingProperties(SessionProperties *sessionProperties);
    ~BindingProperties();

    void addMappings(QDataWidgetMapper &mapper);
    Binding::Type currentType() const;
    Binding::Editor currentEditor() const;
    void setFields(const QStringList &fields);
    const QStringList &fields() const { return mFields; }

signals:
    void fieldsChanged();

private:
    void updateWidgets();
    QVariantList getItemIds() const;

    SessionProperties &mSessionProperties;
    Ui::BindingProperties *mUi;
    QStringList mFields;
    bool mSuspendSetFields{ };
};

#endif // BINDINGPROPERTIES_H
