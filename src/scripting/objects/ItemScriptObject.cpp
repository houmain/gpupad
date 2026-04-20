#include "ItemScriptObject.h"
#include "AppScriptObject.h"
#include "session/SessionModel.h"
#include <QJSEngine>
#include <QJsonObject>

ItemScriptObject::ItemScriptObject(AppScriptObject *appScriptObject,
    ItemId itemId)
    : mAppScriptObject(*appScriptObject)
    , mItemId(itemId)
{
    auto &session = mAppScriptObject.threadSessionModel();
    auto index = session.getIndex(session.findItem(mItemId));
    Q_ASSERT(index.isValid());

    const auto json = session.getJson({ index }).first().toObject();
    for (auto it = json.begin(); it != json.end(); ++it)
        if (it.key() != "items")
            insert(it.key(), it.value().toVariant());

    setItemsList(session.getItem(index).items);
}

void ItemScriptObject::setItemsList(const QList<Item *> &items)
{
    insert("items", QVariant::fromValue(mAppScriptObject.makeArray([&](auto add) {
        for (const auto *item : items)
            add(mAppScriptObject.createItemObject(item->id));
    })));
}

QVariant ItemScriptObject::updateValue(const QString &key,
    const QVariant &input)
{
    auto update = QJsonObject();
    update.insert("id", mItemId);
    if (input.canConvert<QJSValue>()) {
        const auto jsValue = mAppScriptObject.engine().toScriptValue(input);
        const auto json = jsValue.toVariant(QJSValue::ConvertJSObjects);
        update.insert(key, json.toJsonValue());
    } else {
        update.insert(key, input.toJsonValue());
    }

    mAppScriptObject.withSessionModel(
        [itemId = mItemId, update](SessionModel &session) {
            const auto index = session.getIndex(session.findItem(itemId));
            if (index.isValid())
                session.dropJson({ update }, index.row(), index.parent(), true);
        });
    return input;
}
