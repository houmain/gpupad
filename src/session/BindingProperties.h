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

class BindingProperties final : public QWidget
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
    QStringList values() const;

Q_SIGNALS:
    void valuesChanged();

private:
    void updateWidgets();
    int getTextureStride(QVariant textureId) const;
    int getBufferStride(QVariant bufferId) const;
    void filterImageFormats(int stride);

    SessionProperties &mSessionProperties;
    Ui::BindingProperties *mUi;
    QStringList mValues;
    bool mSuspendSetValues{ };
};

#endif // BINDINGPROPERTIES_H
