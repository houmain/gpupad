#pragma once

#include "MessageList.h"
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QJSValue>
#include <functional>

class SessionModel;
class ScriptEngine;
struct Item;

class SessionScriptObject : public QObject
{
    Q_OBJECT

public:
    explicit SessionScriptObject(QObject *parent = nullptr);

    void beginUpdate(ScriptEngine *scriptEndine, SessionModel *sessionCopy);
    void endUpdate();
    void applyUpdate();

    Q_INVOKABLE QJsonArray getItems() const;
    Q_INVOKABLE void updateItems(QJsonValue update);
    Q_INVOKABLE void deleteItem(QJsonValue item);
    Q_INVOKABLE void setBufferData(QJsonValue item, QJSValue data);
    Q_INVOKABLE void setBlockData(QJsonValue item, QJSValue data);
    Q_INVOKABLE void setTextureData(QJsonValue item, QJSValue data);

private:
    const Item *findItem(QJsonValue objectOrId) const;

    MessagePtrSet mMessages;
    SessionModel *mSession{ };
    ScriptEngine *mScriptEngine{ };
    std::vector<std::function<void()>> mPendingUpdates;
    std::vector<std::function<void()>> mPendingEditorUpdates;
};
