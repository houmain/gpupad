#pragma once

#include "MessageList.h"
#include <QJSValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <functional>

class SessionModel;
struct Item;

class SessionScriptObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ sessionName WRITE setSessionName)
    Q_PROPERTY(QJSValue items READ sessionItems CONSTANT)

public:
    explicit SessionScriptObject(QObject *parent = nullptr);
    void initializeEngine(QJSEngine *engine);

    void beginBackgroundUpdate(SessionModel *sessionCopy);
    void endBackgroundUpdate();
    QJSValue getItem(QModelIndex index);

    QString sessionName();
    void setSessionName(QString name);
    QJSValue sessionItems();

    Q_INVOKABLE void clear();
    Q_INVOKABLE QJSValue insertItem(QJSValue object);
    Q_INVOKABLE QJSValue insertItem(QJSValue itemDesc, QJSValue object);
    Q_INVOKABLE QJSValue getItem(QJSValue itemDesc);
    Q_INVOKABLE void clearItem(QJSValue itemDesc);
    Q_INVOKABLE void deleteItem(QJSValue itemDesc);

    Q_INVOKABLE void setBufferData(QJSValue itemDesc, QJSValue data);
    Q_INVOKABLE void setBlockData(QJSValue itemDesc, QJSValue data);
    Q_INVOKABLE void setTextureData(QJSValue itemDesc, QJSValue data);
    Q_INVOKABLE void setScriptSource(QJSValue itemDesc, QJSValue data);
    Q_INVOKABLE void setShaderSource(QJSValue itemDesc, QJSValue data);

private:
    class ItemObject;
    class ItemListObject;
    using UpdateFunction = std::function<void(SessionModel &)>;

    SessionModel &threadSessionModel();
    QJSEngine &engine();
    void withSessionModel(UpdateFunction &&updateFunction);
    ItemId getItemId(QJSValue itemDesc);

    QJSEngine *mEngine{};
    QJSValue mSessionItems;
    MessagePtrSet mMessages;
    SessionModel *mSessionCopy{};
    std::vector<UpdateFunction> mPendingUpdates;
    bool mUpdatedEditor{};
};
