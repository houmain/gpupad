#pragma once

#include "MessageList.h"
#include <QJSValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QJSEngine>
#include <QQmlPropertyMap>
#include <QModelIndexList>
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

    void setItemsList(const QList<Item *> &items);

private:
    QVariant updateValue(const QString &key, const QVariant &input) override;

    SessionScriptObject &mSessionObject;
    ItemId mItemId;
};

class SessionScriptObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJSValue selection READ selection CONSTANT)
    Q_PROPERTY(ItemId id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(QJSValue items READ items CONSTANT)

public:
    explicit SessionScriptObject(QObject *parent = nullptr);
    void initializeEngine(QJSEngine *engine);

    void setSelection(const QModelIndexList &selectedIndices);
    void beginBackgroundUpdate(IScriptRenderSession *renderSession);
    void endBackgroundUpdate();
    bool available() const;

    QJSValue selection() { return mSelectionProperty; }
    ItemId id();
    QString name();
    void setName(QString name);
    QJSValue items();

    Q_INVOKABLE QJSValue getParentItem(QJSValue itemIdent);
    Q_INVOKABLE QJSValue findItem(QJSValue itemIdent);
    Q_INVOKABLE QJSValue findItem(QJSValue itemIdent, QJSValue originIdent,
        bool searchSubItems = false);
    Q_INVOKABLE QJSValue findItems(QJSValue itemIdent, QJSValue originIdent,
        bool searchSubItems = false);
    Q_INVOKABLE QJSValue findItems(QJSValue itemIdent);
    Q_INVOKABLE QJSValue insertItem(QJSValue object);
    Q_INVOKABLE QJSValue insertItem(QJSValue parentIdent, QJSValue object);
    Q_INVOKABLE QJSValue insertBeforeItem(QJSValue siblingIdent,
        QJSValue object);
    Q_INVOKABLE QJSValue insertAfterItem(QJSValue siblingIdent,
        QJSValue object);
    Q_INVOKABLE void replaceItems(QJSValue parentIdent, QJSValue array);
    Q_INVOKABLE void clearItems(QJSValue parentIdent);
    Q_INVOKABLE void deleteItem(QJSValue itemIdent);

    Q_INVOKABLE void setBufferData(QJSValue itemIdent, QJSValue data);
    Q_INVOKABLE void setBlockData(QJSValue itemIdent, QJSValue data);
    Q_INVOKABLE void setTextureData(QJSValue itemIdent, QJSValue data);
    Q_INVOKABLE void setScriptSource(QJSValue itemIdent, QJSValue data);
    Q_INVOKABLE void setShaderSource(QJSValue itemIdent, QJSValue data);

    Q_INVOKABLE quint64 getTextureHandle(QJSValue itemIdent);
    Q_INVOKABLE quint64 getBufferHandle(QJSValue itemIdent);
    Q_INVOKABLE QJSValue processShader(QJSValue itemIdent, QString processType);

private:
    friend SessionScriptObject_ItemObject;
    using ItemObject = SessionScriptObject_ItemObject;
    using UpdateFunction = std::function<void(SessionModel &)>;

    SessionModel &threadSessionModel();
    QJSEngine &engine();
    QJsonObject toJsonObject(const QJSValue &object);
    void withSessionModel(UpdateFunction &&updateFunction);

    const Item *findSessionItem(QJSValue itemIdent,
        const QModelIndex &originIndex, bool searchSubItems);
    const Item *findSessionItem(QJSValue itemIdent, QJSValue originIdent,
        bool searchSubItems);
    const Item *findSessionItem(QJSValue itemIdent);
    QJSValue createItemObject(ItemId itemId);
    void refreshItemObjectItems(const Item *item);
    QJSValue insertItemAt(const Item *parent, int row, QJSValue object);

    template <typename T>
    const T *findSessionItem(const QJSValue &itemObject)
    {
        return castItem<T>(findSessionItem(itemObject));
    }

    template <typename AddElements>
    QJSValue makeArray(AddElements &&addElements)
    {
        auto array = engine().newArray();
        auto i = 0;
        addElements(
            [&](const QJSValue &element) { array.setProperty(i++, element); });
        return array;
    }

    QJSEngine *mEngine{};
    QJSValue mSelectionProperty;
    QJSValue mSessionItems;
    IScriptRenderSession *mRenderSession{};
    std::vector<UpdateFunction> mPendingUpdates;
    std::map<ItemId, std::pair<ItemObject *, QJSValue>> mCreatedItemObjects;
};
