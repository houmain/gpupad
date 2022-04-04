#pragma once

#include "MessageList.h"
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QJSValue>
#include <functional>

class SessionModel;
struct Item;

class SessionScriptObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJSValue items READ rootItems CONSTANT)

public:
    explicit SessionScriptObject(QJSEngine *engine);

    void beginBackgroundUpdate(SessionModel *sessionCopy);
    void endBackgroundUpdate();

    QJSValue rootItems();
    Q_INVOKABLE QJSValue item(QJSValue itemDesc);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void clearItem(QJSValue itemDesc);
    Q_INVOKABLE void deleteItem(QJSValue itemDesc);

    Q_INVOKABLE void setBufferData(QJSValue itemDesc, QJSValue data);
    Q_INVOKABLE void setBlockData(QJSValue itemDesc, QJSValue data);
    Q_INVOKABLE void setTextureData(QJSValue itemDesc, QJSValue data);

    Q_INVOKABLE QJSValue enumerateFiles(const QString &pattern);
    Q_INVOKABLE QJSValue writeTextFile(const QString &fileName, const QString &string);
    Q_INVOKABLE QJSValue readTextFile(const QString &fileName);

private:
    class ItemObject;
    class ItemListObject;
    using UpdateFunction = std::function<void(SessionModel &)>;

    SessionModel &threadSessionModel();
    QJSEngine &engine();
    void withSessionModel(UpdateFunction &&updateFunction);
    ItemId getItemId(QJSValue itemDesc);

    QJSEngine *mEngine{ };
    QJSValue mRootItemsList;
    MessagePtrSet mMessages;
    SessionModel *mSessionCopy{ };
    std::vector<UpdateFunction> mPendingUpdates;
    bool mUpdatedEditor{ };
};
