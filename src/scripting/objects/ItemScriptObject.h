#pragma once

#include "MessageList.h"
#include <QQmlPropertyMap>

class AppScriptObject;
struct Item;

class ItemScriptObject : public QQmlPropertyMap
{
    Q_OBJECT

public:
    ItemScriptObject(AppScriptObject *appScriptObject, ItemId itemId);

    void setItemsList(const QList<Item *> &items);

private:
    QVariant updateValue(const QString &key, const QVariant &input) override;

    AppScriptObject &mAppScriptObject;
    const ItemId mItemId;
};
