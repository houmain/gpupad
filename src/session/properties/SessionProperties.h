#pragma once

#include "../Item.h"
#include <QVariant>
#include <QWidget>
#include <QAbstractItemModel>

namespace Ui {
    class SessionProperties;
}

class QDataWidgetMapper;
class PropertiesEditor;

class VariantMapModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit VariantMapModel(QObject *parent = nullptr);

    void setVariantMap(QVariantMap variantMap) { mVariantMap = variantMap; }
    const QVariantMap &variantMap() const { return mVariantMap; }
    QModelIndex index(int row, int column,
        const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
        int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
        int role = Qt::EditRole) override;
    virtual QString getColumnKey(int column) const = 0;
    virtual QVariant getColumnDefaultValue(int column) const = 0;

private:
    QVariantMap mVariantMap;
};

//-------------------------------------------------------------------------

class ShaderCompilerSettingsModel : public VariantMapModel
{
    Q_OBJECT
public:
    using VariantMapModel::VariantMapModel;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QString getColumnKey(int column) const override;
    QVariant getColumnDefaultValue(int column) const override;
};

//-------------------------------------------------------------------------

class SessionProperties final : public QWidget
{
    Q_OBJECT
public:
    explicit SessionProperties(PropertiesEditor *propertiesEditor);
    ~SessionProperties();

    void addMappings(QDataWidgetMapper &mapper);
    void submitShaderCompilerSettings();

private:
    void updateShaderCompiler();
    void updateWidgets();

    PropertiesEditor &mPropertiesEditor;
    Ui::SessionProperties *mUi;
    ShaderCompilerSettingsModel *mShaderCompilerSettingsModel{};
    QDataWidgetMapper *mShaderCompilerSettingsMapper{};
};
