#pragma once

#include "Item.h"
#include <QVariant>
#include <QWidget>

namespace Ui {
    class BindingProperties;
}

class PropertiesEditor;
class QDataWidgetMapper;

class BindingProperties final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList values READ values WRITE setValues NOTIFY
            valuesChanged USER true)
public:
    explicit BindingProperties(PropertiesEditor *propertiesEditor);
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
    int getBufferStride(QVariant blockId) const;
    void filterImageFormats(int stride);

    PropertiesEditor &mPropertiesEditor;
    Ui::BindingProperties *mUi;
    QStringList mValues;
    bool mSuspendSetValues{};
};
