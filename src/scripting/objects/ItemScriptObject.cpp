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

    QQmlPropertyMap::insert(properties);
    return true;
}

QJSValue ItemScriptObject::insert(QJSValue object)
{
    return mAppScriptObject.insertItem(QJSValue(mItemId), object);
}

QJSValue ItemScriptObject::properties()
{
    return mAppScriptObject.getItemProperties(QJSValue(mItemId));
}

QJSValue ItemScriptObject::parent()
{
    return mAppScriptObject.getParentItem(QJSValue(mItemId));
}

QJSValue ItemScriptObject::getItem(QJSValue itemIdent)
{
    if (auto item = findItem(itemIdent); !item.isUndefined())
        return item;

    mAppScriptObject.throwJsError("Item does not exist");
    return QJSValue::UndefinedValue;
}

QJSValue ItemScriptObject::findItem(QJSValue itemIdent)
{
    return findItem(itemIdent, true);
}

QJSValue ItemScriptObject::findItem(QJSValue itemIdent, bool searchSubItems)
{
    return mAppScriptObject.findItem(itemIdent, QJSValue(mItemId),
        searchSubItems);
}

QJSValue ItemScriptObject::findItems(QJSValue itemIdent)
{
    return findItems(itemIdent, true);
}

QJSValue ItemScriptObject::findItems(QJSValue itemIdent, bool searchSubItems)
{
    return mAppScriptObject.findItems(itemIdent, QJSValue(mItemId),
        searchSubItems);
}

QJSValue ItemScriptObject::trackItems(QJSValue itemIdent, QJSValue callback)
{
    return trackItems(itemIdent, true, callback);
}

QJSValue ItemScriptObject::trackItems(QJSValue itemIdent, bool searchSubItems,
    QJSValue callback)
{
    return mAppScriptObject.trackItems(itemIdent, QJSValue(mItemId),
        searchSubItems, callback);
}

void ItemScriptObject::untrackItems(int trackingIndex)
{
    return mAppScriptObject.untrackItems(trackingIndex);
}

QJSValue ItemScriptObject::insertBefore(QJSValue object)
{
    return mAppScriptObject.insertBeforeItem(QJSValue(mItemId), object);
}

QJSValue ItemScriptObject::insertAfter(QJSValue object)
{
    return mAppScriptObject.insertAfterItem(QJSValue(mItemId), object);
}

void ItemScriptObject::replaceItems(QJSValue array)
{
    mAppScriptObject.replaceItems(QJSValue(mItemId), array);
}

void ItemScriptObject::clear()
{
    if (mItemId == mAppScriptObject.threadSessionModel().sessionItem().id) {
        mAppScriptObject.clearSession();
    } else {
        mAppScriptObject.clearItems(QJSValue(mItemId));
    }
}

void ItemScriptObject::remove()
{
    mAppScriptObject.deleteItem(QJSValue(mItemId));
}

QJSValue ItemScriptObject::openEditor()
{
    return mAppScriptObject.openEditor(QJSValue(mItemId));
}

void ItemScriptObject::setData(QJSValue data)
{
    const auto item = mAppScriptObject.threadSessionModel().findItem(mItemId);
    if (!item)
        return mAppScriptObject.throwJsError("Invalid item");

    switch (item->type) {
    case Item::Type::Buffer:
        return mAppScriptObject.setBufferData(QJSValue(mItemId), data);
    case Item::Type::Block:
        return mAppScriptObject.setBlockData(QJSValue(mItemId), data);
    case Item::Type::Texture:
        return mAppScriptObject.setTextureData(QJSValue(mItemId), data);
    default:
        return mAppScriptObject.throwJsError(
            "cannot set data of " + getItemTypeName(item->type));
    }
}

void ItemScriptObject::setSource(QJSValue source)
{
    const auto item = mAppScriptObject.threadSessionModel().findItem(mItemId);
    if (!item)
        return mAppScriptObject.throwJsError("Invalid item");

    switch (item->type) {
    case Item::Type::Script:
        return mAppScriptObject.setScriptSource(QJSValue(mItemId), source);
    case Item::Type::Shader:
        return mAppScriptObject.setShaderSource(QJSValue(mItemId), source);
    default:
        return mAppScriptObject.throwJsError(
            "cannot set source of " + getItemTypeName(item->type));
    }
}

quint64 ItemScriptObject::getDeviceHandle()
{
    const auto item = mAppScriptObject.threadSessionModel().findItem(mItemId);
    if (!item)
        return 0;

    switch (item->type) {
    case Item::Type::Buffer:
        return mAppScriptObject.getBufferHandle(QJSValue(mItemId));
    case Item::Type::Texture:
        return mAppScriptObject.getTextureHandle(QJSValue(mItemId));
    default:
        mAppScriptObject.throwJsError(
            "cannot obtain device handle of " + getItemTypeName(item->type));
        return 0;
    }
}

QJSValue ItemScriptObject::processShader(QString processType)
{
    return mAppScriptObject.processItemShader(QJSValue(mItemId), processType);
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
                session.dropJson({ update }, index.row(), index.parent());
        });
    return input;
}
