#pragma once

#include "message/MessageList.h"
#include <QJSValue>
#include <QQmlPropertyMap>

class AppScriptObject;

class ItemScriptObject : public QQmlPropertyMap
{
    Q_OBJECT

public:
    ItemScriptObject(AppScriptObject *appScriptObject, ItemId itemId);

    bool updateProperties();

    Q_INVOKABLE QJSValue properties();
    Q_INVOKABLE QJSValue parent();
    Q_INVOKABLE QJSValue getItem(QJSValue itemIdent);
    Q_INVOKABLE QJSValue findItem(QJSValue itemIdent);
    Q_INVOKABLE QJSValue findItem(QJSValue itemIdent, bool searchSubItems);
    Q_INVOKABLE QJSValue findItems(QJSValue itemIdent);
    Q_INVOKABLE QJSValue findItems(QJSValue itemIdent, bool searchSubItems);
    Q_INVOKABLE QJSValue trackItems(QJSValue itemIdent, QJSValue callback);
    Q_INVOKABLE QJSValue trackItems(QJSValue itemIdent, bool searchSubItems,
        QJSValue callback);
    Q_INVOKABLE void untrackItems(int trackingIndex);

    Q_INVOKABLE QJSValue insert(QJSValue object);
    Q_INVOKABLE QJSValue insertBefore(QJSValue object);
    Q_INVOKABLE QJSValue insertAfter(QJSValue object);
    Q_INVOKABLE void replaceItems(QJSValue array);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void remove();

    Q_INVOKABLE QJSValue openEditor();
    Q_INVOKABLE void setData(QJSValue data);
    Q_INVOKABLE void setSource(QJSValue source);
    Q_INVOKABLE quint64 getDeviceHandle();
    Q_INVOKABLE QJSValue processShader(QString processType);

private:
    QVariant updateValue(const QString &key, const QVariant &input) override;

    AppScriptObject &mAppScriptObject;
    const ItemId mItemId;
};
