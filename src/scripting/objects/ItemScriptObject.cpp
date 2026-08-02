#include "ItemScriptObject.h"
#include "AppScriptObject.h"
#include "session/SessionModel.h"
#include <QJSEngine>

ItemScriptObject::ItemScriptObject(AppScriptObject *appScriptObject,
    ItemId itemId)
    : QQmlPropertyMap(this, nullptr)
    , mAppScriptObject(*appScriptObject)
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
    if (auto json = session.getJson({ index }, true); !json.empty())
        if (const auto object = jsonObject(json.front()))
            for (const auto &[key, value] : *object) {
                const auto property = QString::fromUtf8(key.data(),
                    static_cast<qsizetype>(key.size()));
                Q_ASSERT(property != "items");
                properties[property] = variantFromJson(value);
            }

    properties["items"] = QVariant::fromValue(
        mAppScriptObject.makeItemArray(session.getItem(index).items));

    insert(properties);
    return true;
}

QVariant ItemScriptObject::updateValue(const QString &key,
    const QVariant &input)
{
    auto update = JsonObject();
    update["id"] = mItemId;
    if (input.canConvert<QJSValue>()) {
        const auto jsValue = mAppScriptObject.jsEngine().toScriptValue(input);
        const auto json = jsValue.toVariant(QJSValue::ConvertJSObjects);
        update[jsonKey(key)] = jsonFromVariant(json);
    } else {
        update[jsonKey(key)] = jsonFromVariant(input);
    }

    mAppScriptObject.withSessionModel(
        [itemId = mItemId, update](SessionModel &session) {
            const auto index = session.getIndex(session.findItem(itemId));
            if (index.isValid())
                session.dropJson({ update }, index.row(), index.parent(), true);
        });
    return input;
}
