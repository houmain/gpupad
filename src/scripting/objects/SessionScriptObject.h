#pragma once

#include "MessageList.h"
#include <QJSValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QQmlPropertyMap>
#include <functional>

class SessionModel;
class SessionScriptObject;
class IScriptRenderSession;
struct Item;

class SessionScriptObject_ItemObject : public QQmlPropertyMap
{
    Q_OBJECT

public:
    SessionScriptObject_ItemObject(SessionScriptObject *sessionObject,
        ItemId itemId);

private:
    QVariant updateValue(const QString &key, const QVariant &input) override;

    SessionScriptObject &mSessionObject;
    ItemId mItemId;
};

class SessionScriptObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ItemId id READ itemId CONSTANT)
    Q_PROPERTY(QString name READ sessionName WRITE setSessionName)
    Q_PROPERTY(QJSValue items READ sessionItems CONSTANT)

public:
    explicit SessionScriptObject(QObject *parent = nullptr);
    void initializeEngine(QJSEngine *engine);

    void beginBackgroundUpdate(IScriptRenderSession *renderSession);
    void endBackgroundUpdate();
    bool available() const;
    QJSValue getItem(QModelIndex index);

    ItemId itemId();
    QString sessionName();
    void setSessionName(QString name);
    QJSValue sessionItems();

    Q_INVOKABLE QJSValue item(QJSValue itemDesc);
    Q_INVOKABLE QJSValue insertItem(QJSValue object);
    Q_INVOKABLE QJSValue insertItem(QJSValue itemDesc, QJSValue object);
    Q_INVOKABLE void replaceItems(QJSValue itemDesc, QJSValue array);
    Q_INVOKABLE void clearItems();
    Q_INVOKABLE void clearItems(QJSValue itemDesc);
    Q_INVOKABLE void deleteItem(QJSValue itemDesc);

    Q_INVOKABLE void setBufferData(QJSValue itemDesc, QJSValue data);
    Q_INVOKABLE void setBlockData(QJSValue itemDesc, QJSValue data);
    Q_INVOKABLE void setTextureData(QJSValue itemDesc, QJSValue data);
    Q_INVOKABLE void setScriptSource(QJSValue itemDesc, QJSValue data);
    Q_INVOKABLE void setShaderSource(QJSValue itemDesc, QJSValue data);

    Q_INVOKABLE quint64 getTextureHandle(QJSValue itemDesc);
    Q_INVOKABLE quint64 getBufferHandle(QJSValue itemDesc);

private:
    friend SessionScriptObject_ItemObject;
    using ItemObject = SessionScriptObject_ItemObject;
    using UpdateFunction = std::function<void(SessionModel &)>;

    SessionModel &threadSessionModel();
    QJSEngine &engine();
    QJsonObject toJsonObject(const QJSValue &object);
    void withSessionModel(UpdateFunction &&updateFunction);
    ItemId getItemId(const QJSValue &itemDesc);
    const Item *getItem(const QJSValue &itemDesc);

    template <typename T>
    const T *getItem(const QJSValue &itemDesc)
    {
        return castItem<T>(getItem(itemDesc));
    }

    QJSEngine *mEngine{};
    QJSValue mSessionItems;
    MessagePtrSet mMessages;
    IScriptRenderSession *mRenderSession{};
    std::vector<UpdateFunction> mPendingUpdates;
};
