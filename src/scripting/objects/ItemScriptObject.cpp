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
    updateProperties();
}

bool ItemScriptObject::updateProperties()
{
    auto &session = mAppScriptObject.threadSessionModel();
    auto index = session.getIndex(session.findItem(mItemId));
    if (!index.isValid())
        return false;

    auto properties = QVariantHash();
    const auto json = session.getJson({ index }, true).first().toObject();
    for (auto it = json.begin(); it != json.end(); ++it) {
        Q_ASSERT(it.key() != "items");
        properties[it.key()] = it.value().toVariant();
    }

    properties["items"] = QVariant::fromValue(
        mAppScriptObject.makeItemArray(session.getItem(index).items));

    insert(properties);
    return true;
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
