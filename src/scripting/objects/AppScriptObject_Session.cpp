#include "AppScriptObject.h"
#include "ItemScriptObject.h"
#include "SynchronizeLogic.h"
#include "FileCache.h"
#include "Singletons.h"
#include "editors/EditorManager.h"
#include "editors/source/SourceEditor.h"
#include "session/SessionModel.h"
#include "LibraryScriptObject.h"
#include "../IScriptRenderSession.h"
#include "render/ProcessSource.h"
#include <QFloat16>
#include <QJSEngine>
#include <QTextStream>
#include <limits>
#include <cstring>

namespace {
    struct Int64
    {
        int64_t v;
        Int64(const QString &string) { v = string.toLongLong(); }
    };

    struct Uint64
    {
        uint64_t v;
        Uint64(const QString &string) { v = string.toULongLong(); }
    };

    QByteArray toByteArray(const QJSValue &data, const Block &block)
    {
        auto elementTypes = std::vector<Field::DataType>();
        auto padding = std::vector<int>();
        for (auto item : block.items) {
            auto column = static_cast<const Field *>(item);
            for (auto i = 0; i < column->count; ++i) {
                elementTypes.push_back(column->dataType);
                padding.push_back(0);
            }
            if (column->padding) {
                if (padding.empty())
                    return {};
                padding.back() += column->padding;
            }
        }
        if (elementTypes.empty())
            return {};

        auto bytes = QByteArray();
        if (auto array = qobject_cast<const LibraryScriptObject_Array *>(
                data.toQObject())) {

            const auto rowCount = array->length() / elementTypes.size();
            bytes.resize(getBlockStride(block) * rowCount);

            const auto writeRows = [&](const auto *values) {
                auto pos = bytes.data();
                const auto write = [&](auto v) {
                    std::memcpy(pos, &v, sizeof(v));
                    pos += sizeof(v);
                };
                auto index = 0u;
                for (auto i = 0u; i < rowCount; ++i) {
                    for (auto j = 0u; j < elementTypes.size(); ++j, ++index) {
                        const auto value = values[index];
                        switch (elementTypes[j]) {
#define ADD(TYPE, T)                  \
    case TYPE: {                      \
        write(static_cast<T>(value)); \
    } break;
                            ADD(Field::DataType::Int8, int8_t)
                            ADD(Field::DataType::Uint8, uint8_t)
                            ADD(Field::DataType::Int16, int16_t)
                            ADD(Field::DataType::Uint16, uint16_t)
                            ADD(Field::DataType::Int32, int32_t)
                            ADD(Field::DataType::Uint32, uint32_t)
                            ADD(Field::DataType::Int64, int64_t)
                            ADD(Field::DataType::Uint64, uint64_t)
                            ADD(Field::DataType::Float, float)
                            ADD(Field::DataType::Double, double)
#undef ADD
                        }
                        for (auto i = 0; i < padding[j]; ++i)
                            write(uint8_t{ 0 });
                    }
                }
            };

            switch (array->type()) {
#define ADD(TYPE, T)                 \
    case TYPE: {                     \
        writeRows(array->data<T>()); \
    } break;
                ADD(dllreflect::Type::Bool, bool)
                ADD(dllreflect::Type::Char, char)
                ADD(dllreflect::Type::Int8, int8_t)
                ADD(dllreflect::Type::UInt8, uint8_t)
                ADD(dllreflect::Type::Int16, int16_t)
                ADD(dllreflect::Type::UInt16, uint16_t)
                ADD(dllreflect::Type::Int32, int32_t)
                ADD(dllreflect::Type::UInt32, uint32_t)
                ADD(dllreflect::Type::Int64, int64_t)
                ADD(dllreflect::Type::UInt64, uint64_t)
                ADD(dllreflect::Type::Float, float)
                ADD(dllreflect::Type::Double, double)
#undef ADD
            case dllreflect::Type::Void: break;
            }
        } else if (data.isArray()) {
            // there does not seems to be a way to access e.g. Float32Array directly...
            const auto rowCount = data.property("length").toInt()
                / elementTypes.size();
            bytes.resize(getBlockStride(block) * rowCount);

            auto pos = bytes.data();
            const auto write = [&](auto v) {
                std::memcpy(pos, &v, sizeof(v));
                pos += sizeof(v);
            };

            auto index = 0u;
            for (auto i = 0u; i < rowCount; ++i) {
                for (auto j = 0u; j < elementTypes.size(); ++j, ++index) {
                    const auto value = data.property(index);
                    switch (elementTypes[j]) {
#define ADD(TYPE, T, GET)                   \
    case TYPE: {                            \
        write(static_cast<T>(value.GET())); \
    } break;
                        ADD(Field::DataType::Int8, int8_t, toInt)
                        ADD(Field::DataType::Uint8, uint8_t, toUInt)
                        ADD(Field::DataType::Int16, int16_t, toInt)
                        ADD(Field::DataType::Uint16, uint16_t, toUInt)
                        ADD(Field::DataType::Int32, int32_t, toInt)
                        ADD(Field::DataType::Uint32, uint32_t, toUInt)
                        ADD(Field::DataType::Int64, Int64, toString)
                        ADD(Field::DataType::Uint64, Uint64, toString)
                        ADD(Field::DataType::Float, float, toNumber)
                        ADD(Field::DataType::Double, double, toNumber)
#undef ADD
                    }
                    for (auto i = 0; i < padding[j]; ++i)
                        write(uint8_t{ 0 });
                }
            }
        }
        return bytes;
    } // namespace

    template <typename T>
    T denormalize(double number)
    {
        if (number < 0)
            return static_cast<T>(
                std::min(-number, 1.0) * std::numeric_limits<T>::min());
        return static_cast<T>(
            std::min(number, 1.0) * std::numeric_limits<T>::max());
    }

    TextureData toTextureData(const QJSValue &data, const Texture &texture)
    {
        auto textureData = TextureData();
        if (!textureData.create(texture.target, texture.format,
                texture.width.toInt(), texture.height.toInt(),
                texture.depth.toInt(), texture.layers.toInt()))
            return {};

        const auto components = getTextureComponentCount(texture.format);
        const auto count = (textureData.width() * textureData.height()
            * textureData.depth() * components);

        auto pos = textureData.getWriteonlyData(0, 0, 0);
        const auto write = [&](auto v) {
            std::memcpy(pos, &v, sizeof(v));
            pos += sizeof(v);
        };
        for (auto i = 0; i < count; ++i) {
            const auto value = data.property(i);
            switch (textureData.format()) {
            case QOpenGLTexture::R8_UNorm:
            case QOpenGLTexture::RG8_UNorm:
            case QOpenGLTexture::RGB8_UNorm:
            case QOpenGLTexture::RGBA8_UNorm:
                write(denormalize<uint8_t>(value.toNumber()));
                break;

            case QOpenGLTexture::R8U:
            case QOpenGLTexture::RG8U:
            case QOpenGLTexture::RGB8U:
            case QOpenGLTexture::RGBA8U:
                write(static_cast<uint8_t>(value.toUInt()));
                break;

            case QOpenGLTexture::R16_UNorm:
            case QOpenGLTexture::RG16_UNorm:
            case QOpenGLTexture::RGB16_UNorm:
            case QOpenGLTexture::RGBA16_UNorm:
                write(denormalize<uint16_t>(value.toNumber()));
                break;

            case QOpenGLTexture::R16U:
            case QOpenGLTexture::RG16U:
            case QOpenGLTexture::RGB16U:
            case QOpenGLTexture::RGBA16U:
                write(static_cast<uint16_t>(value.toUInt()));
                break;

            case QOpenGLTexture::R32U:
            case QOpenGLTexture::RG32U:
            case QOpenGLTexture::RGB32U:
            case QOpenGLTexture::RGBA32U:
                write(static_cast<uint32_t>(value.toUInt()));
                break;

            case QOpenGLTexture::R8_SNorm:
            case QOpenGLTexture::RG8_SNorm:
            case QOpenGLTexture::RGB8_SNorm:
            case QOpenGLTexture::RGBA8_SNorm:
                write(denormalize<uint8_t>(value.toNumber()));
                break;

            case QOpenGLTexture::R8I:
            case QOpenGLTexture::RG8I:
            case QOpenGLTexture::RGB8I:
            case QOpenGLTexture::RGBA8I:
                write(static_cast<int8_t>(value.toInt()));
                break;

            case QOpenGLTexture::R16_SNorm:
            case QOpenGLTexture::RG16_SNorm:
            case QOpenGLTexture::RGB16_SNorm:
            case QOpenGLTexture::RGBA16_SNorm:
                write(denormalize<int16_t>(value.toNumber()));
                break;

            case QOpenGLTexture::R16I:
            case QOpenGLTexture::RG16I:
            case QOpenGLTexture::RGB16I:
            case QOpenGLTexture::RGBA16I:
                write(static_cast<int16_t>(value.toInt()));
                break;

            case QOpenGLTexture::R32I:
            case QOpenGLTexture::RG32I:
            case QOpenGLTexture::RGB32I:
            case QOpenGLTexture::RGBA32I:
                write(static_cast<int32_t>(value.toInt()));
                break;

            case QOpenGLTexture::R16F:
            case QOpenGLTexture::RG16F:
            case QOpenGLTexture::RGB16F:
            case QOpenGLTexture::RGBA16F:
                write(static_cast<qfloat16>(value.toNumber()));
                break;

            case QOpenGLTexture::R32F:
            case QOpenGLTexture::RG32F:
            case QOpenGLTexture::RGB32F:
            case QOpenGLTexture::RGBA32F:
                write(static_cast<float>(value.toNumber()));
                break;

            default: return {};
            }
        }
        return textureData;
    }

    void ensureFileName(SessionModel &session, const FileItem &item,
        QString *hint)
    {
        auto fileName = item.fileName;
        if (fileName.isEmpty() && hint)
            fileName = *hint;
        if (fileName.isEmpty())
            fileName = FileDialog::generateNextUntitledFileName(item.name);
        const auto index = session.getIndex(&item, SessionModel::FileName);
        session.setData(index, fileName);
        if (hint)
            *hint = fileName;
    }
} // namespace

bool AppScriptObject::isSessionAvailable() const
{
    return (onMainThread() || mRenderSession);
}

SessionModel &AppScriptObject::threadSessionModel()
{
    if (onMainThread()) {
        return Singletons::sessionModel();
    } else {
        Q_ASSERT(mRenderSession);
        return mRenderSession->sessionModelCopy();
    }
}

void AppScriptObject::setSelection(const QModelIndexList &selectedIndices)
{
    auto itemIds = QList<ItemId>();
    for (const auto &index : selectedIndices)
        itemIds.append(threadSessionModel().getItem(index).id);
    mSelectionProperty = makeItemArray(itemIds);
}

void AppScriptObject::withSessionModel(UpdateSessionFunction &&updateFunction)
{
    if (onMainThread()) {
        updateFunction(Singletons::sessionModel());
    } else {
        Q_ASSERT(mRenderSession);
        updateFunction(mRenderSession->sessionModelCopy());
        mPendingSessionUpdates.push_back(std::move(updateFunction));
    }
}

void AppScriptObject::beginBackgroundUpdate(IScriptRenderSession *renderSession)
{
    Q_ASSERT(!onMainThread());
    mRenderSession = renderSession;
}

void AppScriptObject::endBackgroundUpdate()
{
    Q_ASSERT(onMainThread());
    mRenderSession = nullptr;

    auto &session = Singletons::sessionModel();
    if (!mPendingSessionUpdates.empty()) {
        session.beginUndoMacro("Script");
        for (const auto &update : mPendingSessionUpdates)
            update(session);
        mPendingSessionUpdates.clear();
        session.endUndoMacro();
    }

    Singletons::fileCache().updateFromEditors();
}

QJSValue AppScriptObject::session()
{
    if (!isSessionAvailable()) {
        engine().throwError(QJSValue::EvalError, "Session not available");
        return QJSValue::UndefinedValue;
    }

    if (mSessionProperty.isUndefined())
        mSessionProperty =
            makeItemObject(threadSessionModel().sessionItem().id);
    return mSessionProperty;
}

QJSValue AppScriptObject::insertItem(QJSValue object)
{
    return insertItem(QJSValue::UndefinedValue, object);
}

QJsonObject AppScriptObject::toJsonObject(const QJSValue &value)
{
    if (auto object = value.toQObject()) {
        // create JSON object from ItemObject
        if (const auto item = qobject_cast<const QQmlPropertyMap *>(object)) {
            auto jsonObject = QJsonObject();
            const auto keys = item->keys();
            for (const auto &key : keys)
                jsonObject.insert(key,
                    QJsonValue::fromVariant(item->value(key)));
            return jsonObject;
        }
        return {};
    }
    return value.toVariant().toJsonObject();
}

QJSValue AppScriptObject::insertItemAt(const Item *parent, int row,
    QJSValue object)
{
    auto id = object.property("id").toInt();
    if (!id) {
        // update ID inplace
        id = threadSessionModel().getNextItemId();
        object.setProperty("id", id);
    }

    const auto update = toJsonObject(object);
    if (update.isEmpty()) {
        engine().throwError(QStringLiteral("Invalid object"));
        return QJSValue::UndefinedValue;
    }

    withSessionModel(
        [parentId = parent->id, row, update](SessionModel &session) {
            const auto parent = session.getIndex(session.findItem(parentId));
            session.dropJson({ update }, row, parent, true);
        });
    updateItemProperties(parent);

    return makeItemObject(id);
}

QJSValue AppScriptObject::insertItem(QJSValue parentIdent, QJSValue object)
{
    const auto parent = (parentIdent.isUndefined()
            ? &threadSessionModel().sessionItem()
            : findSessionItem(parentIdent));
    if (!parent) {
        engine().throwError(QStringLiteral("Invalid parent"));
        return QJSValue::UndefinedValue;
    }
    return insertItemAt(parent, parent->items.size(), object);
}

QJSValue AppScriptObject::insertBeforeItem(QJSValue siblingIdent,
    QJSValue object)
{
    const auto sibling = findSessionItem(siblingIdent);
    if (!sibling || !sibling->parent) {
        engine().throwError(QStringLiteral("Invalid sibling"));
        return QJSValue::UndefinedValue;
    }
    const auto parent = sibling->parent;
    const auto row = parent->items.indexOf(sibling);
    return insertItemAt(parent, row, object);
}

QJSValue AppScriptObject::insertAfterItem(QJSValue siblingIdent,
    QJSValue object)
{
    const auto sibling = findSessionItem(siblingIdent);
    if (!sibling || !sibling->parent) {
        engine().throwError(QStringLiteral("Invalid sibling"));
        return QJSValue::UndefinedValue;
    }
    const auto parent = sibling->parent;
    const auto row = parent->items.indexOf(sibling) + 1;
    return insertItemAt(parent, row, object);
}

void AppScriptObject::replaceItems(QJSValue parentIdent, QJSValue array)
{
    const auto count = array.property("length").toInt();
    auto update = QJsonArray();
    for (auto i = 0; i < count; ++i)
        if (!array.property(i).isUndefined()) {
            const auto object = toJsonObject(array.property(i));
            if (object.isEmpty()) {
                engine().throwError(QStringLiteral("Invalid object"));
                return;
            }
            update.append(object);
        }

    const auto parent = findSessionItem(parentIdent);
    if (!parent)
        return;

    // collect Items in current list
    struct ItemInfo
    {
        ItemId id;
        Item::Type type;
        bool hasItems;
    };
    auto unusedItems = std::vector<ItemInfo>();
    for (const auto &item : parent->items)
        unusedItems.push_back(
            ItemInfo{ item->id, item->type, !item->items.empty() });

    // remove Items contained in new list
    for (auto i = 0; i < update.size(); ++i)
        if (const auto id = update[i].toObject()["id"].toInt()) {
            const auto it = std::find_if(unusedItems.begin(), unusedItems.end(),
                [&](const ItemInfo &item) { return item.id == id; });
            if (it != unusedItems.end())
                unusedItems.erase(it);
        }

    // reuse Items with same type and no sub items
    for (auto i = 0; i < update.size(); ++i) {
        auto object = update[i].toObject();
        if (!object["id"].toInt()) {
            const auto type = getItemTypeByName(object["type"].toString());
            const auto it = std::find_if(unusedItems.begin(), unusedItems.end(),
                [&](const ItemInfo &item) {
                    return item.type == type && !item.hasItems;
                });
            if (it != unusedItems.end()) {
                object["id"] = it->id;
                update[i] = object;
                unusedItems.erase(it);
            }
        }
    }

    withSessionModel(
        [parentId = parent->id, update, unusedItems](SessionModel &session) {
            for (const auto &item : unusedItems)
                session.deleteItem(session.getIndex(session.findItem(item.id)));
            const auto parent = session.getIndex(session.findItem(parentId));
            session.dropJson(update, session.rowCount(parent), parent, true);
        });

    updateItemProperties(parent);

    // update id in place
    Q_ASSERT(parent->items.size() == update.size());
    for (auto i = 0; i < parent->items.size(); ++i)
        array.property(i).setProperty("id", parent->items[i]->id);
}

QModelIndex AppScriptObject::getOriginIndex(QJSValue originIdent)
{
    const auto &session = threadSessionModel();
    if (originIdent.isUndefined())
        return session.sessionItemIndex();

    const auto origin = findSessionItem(originIdent);
    if (!origin) {
        engine().throwError(QStringLiteral("Invalid origin"));
        return {};
    }
    return session.getIndex(origin);
}

const Item *AppScriptObject::findSessionItem(QJSValue itemIdent,
    const QModelIndex &originIndex, bool searchSubItems)
{
    const auto &session = threadSessionModel();
    if (itemIdent.isString()) {
        const auto parts = itemIdent.toString().split('/');
        auto index = originIndex;
        for (const auto &part : parts) {
            index = session.findChildByName(index, part);
            if (!index.isValid())
                return nullptr;
        }
        return &session.getItem(index);
    }

    if (itemIdent.isCallable()) {
        auto result = std::add_pointer_t<const Item>{};
        const auto callback = [&](const Item &item) {
            if (!result && item.type != Item::Type::Root)
                if (itemIdent.call({ makeItemObject(item.id) }).toBool())
                    result = session.findItem(item.id);
        };
        if (searchSubItems) {
            session.forEachItem(originIndex, callback);
        } else {
            session.forEachItemDirect(originIndex, callback);
        }
        return result;
    }

    const auto itemId = (itemIdent.isNumber()
            ? static_cast<ItemId>(itemIdent.toNumber())
            : itemIdent.property("id").toInt());
    if (!itemId || !session.findItem(itemId))
        return nullptr;
    return session.findItem(itemId);
}

const Item *AppScriptObject::findSessionItem(QJSValue itemIdent)
{
    return findSessionItem(itemIdent, threadSessionModel().sessionItemIndex(),
        true);
}

QList<const Item *> AppScriptObject::findSessionItems(QJSValue itemIdent,
    const QModelIndex &originIndex, bool searchSubItems)
{
    auto items = QList<const Item *>();
    if (itemIdent.isCallable()) {
        const auto callback = [&](const Item &item) {
            if (item.type != Item::Type::Root) {
                auto itemObject = makeItemObject(item.id);
                if (itemIdent.call({ itemObject }).toBool())
                    items.append(&item);
            }
        };
        if (searchSubItems) {
            threadSessionModel().forEachItem(originIndex, callback);
        } else {
            threadSessionModel().forEachItemDirect(originIndex, callback);
        }
    } else {
        if (auto item = findSessionItem(itemIdent, originIndex, searchSubItems))
            items.append(item);
    }
    return items;
}

QJSValue AppScriptObject::makeItemObject(ItemId itemId)
{
    Q_ASSERT(itemId);
    auto &[object, jsValue] = mItemObjects[itemId];
    if (!object) {
        object = new ItemScriptObject(this, itemId);
        jsValue = engine().newQObject(object);
    }
    return jsValue;
}

QJSValue AppScriptObject::makeItemArray(const QList<ItemId> &itemIds)
{
    auto array = engine().newArray(itemIds.size());
    auto i = 0u;
    for (auto id : itemIds)
        array.setProperty(i++, makeItemObject(id));
    return array;
}

QJSValue AppScriptObject::makeItemArray(const QList<const Item *> &items)
{
    auto array = engine().newArray(items.size());
    auto i = 0u;
    for (auto item : items)
        array.setProperty(i++, makeItemObject(item->id));
    return array;
}

QJSValue AppScriptObject::makeItemArray(const QList<Item *> &items)
{
    auto array = engine().newArray(items.size());
    auto i = 0u;
    for (auto item : items)
        array.setProperty(i++, makeItemObject(item->id));
    return array;
}

void AppScriptObject::handleItemAdded(const Item *item)
{
    updateItemTracking(item, false);
}

void AppScriptObject::handleItemModified(const Item *item)
{
    if (isSessionAvailable())
        updateItemProperties(item);

    updateItemTracking(item, false);
}

void AppScriptObject::handleItemRemoved(const Item *item)
{
    updateItemTracking(item, true);
}

bool AppScriptObject::itemMatchesFilter(const Item *item,
    const ItemTracking &tracking)
{
    auto parent = item->parent;
    if (!parent || parent->id != tracking.originId) {
        if (!tracking.searchSubItems)
            return false;

        for (;;) {
            parent = parent->parent;
            if (!parent)
                return false;
            if (parent->id == tracking.originId)
                break;
        }
    }

    const auto &itemIdent = tracking.itemIdent;
    if (itemIdent.isCallable())
        return itemIdent.call({ makeItemObject(item->id) }).toBool();

    const auto itemId = (itemIdent.isNumber()
            ? static_cast<ItemId>(itemIdent.toNumber())
            : itemIdent.property("id").toInt());
    return (item->id == itemId);
}

void AppScriptObject::updateItemTracking(const Item *item, bool removed)
{
    for (auto &[id, tracking] : mItemTrackings) {
        const auto matches = itemMatchesFilter(item, tracking);
        const auto added = tracking.addedItemIds.contains(item->id);
        auto action = "modified";
        if ((removed && !added) || (!matches && !added)) {
            continue;
        } else if ((removed && added) || (!matches && added)) {
            tracking.addedItemIds.remove(item->id);
            action = "removed";
        } else if (matches && !added) {
            tracking.addedItemIds.insert(item->id);
            action = "added";
        }
        auto itemObject = makeItemObject(item->id);
        tracking.callback.call({ itemObject, action });
    }
}

void AppScriptObject::updateItemProperties(const Item *item)
{
    if (!item)
        return;

    const auto it = mItemObjects.find(item->id);
    if (it != mItemObjects.end())
        if (!it->second.object->updateProperties())
            mItemObjects.erase(it);
}

QJSValue AppScriptObject::getParentItem(QJSValue itemIdent)
{
    const auto item = findSessionItem(itemIdent);
    if (!item || !item->parent || !item->parent->id)
        return QJSValue::UndefinedValue;

    return makeItemObject(item->parent->id);
}

QJSValue AppScriptObject::findItem(QJSValue itemIdent, QJSValue originIdent,
    bool searchSubItems)
{
    const auto originIndex = getOriginIndex(originIdent);
    if (!originIndex.isValid())
        return QJSValue::UndefinedValue;

    if (auto item = findSessionItem(itemIdent, originIndex, searchSubItems))
        return makeItemObject(item->id);

    return QJSValue::UndefinedValue;
}

QJSValue AppScriptObject::findItem(QJSValue itemIdent)
{
    if (auto item = findSessionItem(itemIdent))
        return makeItemObject(item->id);
    return QJSValue::UndefinedValue;
}

QJSValue AppScriptObject::findItems(QJSValue itemIdent, QJSValue originIdent,
    bool searchSubItems)
{
    const auto originIndex = getOriginIndex(originIdent);
    if (!originIndex.isValid())
        return QJSValue::UndefinedValue;

    return makeItemArray(
        findSessionItems(itemIdent, originIndex, searchSubItems));
}

QJSValue AppScriptObject::findItems(QJSValue itemIdent)
{
    return findItems(itemIdent, QJSValue::UndefinedValue, true);
}

QJSValue AppScriptObject::trackItems(QJSValue itemIdent, QJSValue originIdent,
    bool searchSubItems, QJSValue callback)
{
    if (!callback.isCallable()) {
        engine().throwError(QStringLiteral("Callable expected"));
        return QJSValue::UndefinedValue;
    }

    const auto originIndex = getOriginIndex(originIdent);
    if (!originIndex.isValid())
        return QJSValue::UndefinedValue;

    auto items = findSessionItems(itemIdent, originIndex, searchSubItems);
    auto itemIds = QSet<ItemId>();
    for (const auto *item : items) {
        auto itemObject = makeItemObject(item->id);
        callback.call({ itemObject, "added" });
        itemIds.insert(item->id);
    }

    const auto index = mNextItemTrackingIndex++;
    mItemTrackings[index] = {
        .itemIdent = itemIdent,
        .callback = callback,
        .originId = threadSessionModel().getItemId(originIndex),
        .searchSubItems = searchSubItems,
        .addedItemIds = itemIds,
    };
    return index;
}

QJSValue AppScriptObject::trackItems(QJSValue itemIdent, QJSValue originIdent,
    QJSValue callback)
{
    return trackItems(itemIdent, originIdent, false, callback);
}

QJSValue AppScriptObject::trackItems(QJSValue itemIdent, QJSValue callback)
{
    return trackItems(itemIdent, QJSValue::UndefinedValue, true, callback);
}

void AppScriptObject::clearSession()
{
    clearItems(threadSessionModel().sessionItem().id);
}

void AppScriptObject::clearItems(QJSValue parentIdent)
{
    const auto parent = findSessionItem(parentIdent);
    if (!parent)
        return engine().throwError(QStringLiteral("Invalid parent"));

    withSessionModel([parentId = parent->id](SessionModel &session) {
        const auto index = session.getIndex(session.findItem(parentId));
        if (index.isValid())
            session.removeRows(0, session.rowCount(index), index);
    });
    updateItemProperties(parent);
}

void AppScriptObject::deleteItem(QJSValue itemIdent)
{
    const auto item = findSessionItem(itemIdent);
    if (!item)
        return;

    const auto parent = item->parent;
    withSessionModel([itemId = item->id](SessionModel &session) {
        const auto index = session.getIndex(session.findItem(itemId));
        if (index.isValid())
            session.deleteItem(index);
    });
    updateItemProperties(parent);
}

QJSValue AppScriptObject::openEditor(QJSValue fileNameOrItemIdent)
{
    if (fileNameOrItemIdent.isString()) {
        const auto fileName = getAbsolutePath(fileNameOrItemIdent.toString());
        if (auto editor = tryGetEditorObject(fileName); !editor.isUndefined())
            return editor;

        auto opened = false;
        dispatchToMainThread([&, onMainThread = onMainThread()]() {
            if (fileName.endsWith(".qml", Qt::CaseInsensitive)) {
                const auto enginePtr =
                    (onMainThread ? mEnginePtr.lock() : nullptr);
                const auto editor = Singletons::editorManager().openQmlView(
                    fileName, enginePtr);
                opened = static_cast<bool>(editor);
            } else {
                opened = static_cast<bool>(
                    Singletons::editorManager().openEditor(fileName));
            }
        });
        return (opened ? getEditorObject(fileName) : QJSValue::UndefinedValue);
    }

    const auto item = findSessionItem<FileItem>(fileNameOrItemIdent);
    if (!item)
        return QJSValue::UndefinedValue;

    const auto fileName = std::make_shared<QString>();
    withSessionModel(
        [itemId = item->id, fileName](SessionModel &session) mutable {
            if (const auto item = session.findItem<FileItem>(itemId)) {
                ensureFileName(session, *item, fileName.get());

                if (onMainThread()) {
                    auto &editors = Singletons::editorManager();
                    editors.setAutoRaise(false);
                    editors.openEditor(*item);
                    editors.setAutoRaise(true);
                }
            }
        });
    if (fileName->isEmpty())
        return {};
    return openEditor(*fileName);
}

void AppScriptObject::setBufferData(QJSValue itemIdent, QJSValue data)
{
    const auto buffer = findSessionItem<Buffer>(itemIdent);
    if (!buffer)
        return engine().throwError(QStringLiteral("Invalid Buffer item"));

    if (buffer->items.empty() || buffer->items[0]->items.empty())
        return engine().throwError(
            QStringLiteral("Buffer structure not defined"));

    const auto block = castItem<Block>(buffer->items[0]);

    const auto bufferData = toByteArray(data, *block);
    if (bufferData.isNull())
        return engine().throwError(QStringLiteral("Invalid data"));

    const auto rowCount = bufferData.size() / getBlockStride(*block);

    withSessionModel([bufferId = buffer->id, bufferData, rowCount,
                         fileName = QString()](SessionModel &session) mutable {
        if (auto buffer = session.findItem<Buffer>(bufferId)) {
            if (buffer->items.size() == 1) {
                const auto block = castItem<Block>(buffer->items[0]);
                session.setData(
                    session.getIndex(block, SessionModel::BlockRowCount),
                    rowCount);
            }
            ensureFileName(session, *buffer, &fileName);
            if (onMainThread())
                Singletons::fileCache().updateBinary(buffer->fileName,
                    bufferData);
        }
    });
}

void AppScriptObject::setBlockData(QJSValue itemIdent, QJSValue data)
{
    const auto block = findSessionItem<Block>(itemIdent);
    if (!block)
        return engine().throwError(QStringLiteral("Invalid Block item"));

    const auto bufferData = toByteArray(data, *block);
    if (bufferData.isNull())
        return engine().throwError(QStringLiteral("Invalid data"));

    const auto rowCount = bufferData.size() / getBlockStride(*block);

    withSessionModel([blockId = block->id, bufferData, rowCount,
                         fileName = QString()](SessionModel &session) mutable {
        if (auto block = session.findItem<Block>(blockId))
            if (auto buffer = castItem<Buffer>(block->parent)) {
                session.setData(
                    session.getIndex(block, SessionModel::BlockRowCount),
                    rowCount);
                ensureFileName(session, *buffer, &fileName);
                if (onMainThread()) {
                    auto offset = 0, rowCount = 0;
                    Singletons::synchronizeLogic().evaluateBlockProperties(
                        *block, &offset, &rowCount);
                    Singletons::fileCache().updateBinaryRange(buffer->fileName,
                        offset, bufferData);
                }
            }
    });
}

void AppScriptObject::setTextureData(QJSValue itemIdent, QJSValue data)
{
    const auto texture = findSessionItem<Texture>(itemIdent);
    if (!texture)
        return engine().throwError(QStringLiteral("Invalid Texture item"));

    const auto textureData = toTextureData(data, *texture);
    if (textureData.isNull())
        return engine().throwError(QStringLiteral("Invalid data"));

    withSessionModel([textureId = texture->id, textureData,
                         fileName = QString()](SessionModel &session) mutable {
        if (auto texture = session.findItem<Texture>(textureId)) {
            ensureFileName(session, *texture, &fileName);
            if (onMainThread())
                Singletons::fileCache().updateTexture(texture->fileName,
                    texture->flipVertically, textureData);
        }
    });
}

void AppScriptObject::setShaderSource(QJSValue itemIdent, QJSValue data)
{
    const auto shader = findSessionItem<Shader>(itemIdent);
    if (!shader)
        return engine().throwError(QStringLiteral("Invalid Shader item"));

    withSessionModel([shaderId = shader->id, data = data.toString(),
                         fileName = QString()](SessionModel &session) mutable {
        if (auto shader = session.findItem<Shader>(shaderId)) {
            ensureFileName(session, *shader, &fileName);
            if (onMainThread())
                Singletons::fileCache().updateSource(shader->fileName, data);
        }
    });
}

void AppScriptObject::setScriptSource(QJSValue itemIdent, QJSValue data)
{
    const auto script = findSessionItem<Script>(itemIdent);
    if (!script)
        return engine().throwError(QStringLiteral("Invalid Script item"));

    withSessionModel([scriptId = script->id, data = data.toString(),
                         fileName = QString()](SessionModel &session) mutable {
        if (auto script = session.findItem<Script>(scriptId)) {
            ensureFileName(session, *script, &fileName);
            if (onMainThread())
                Singletons::fileCache().updateSource(script->fileName, data);
        }
    });
}

quint64 AppScriptObject::getTextureHandle(QJSValue itemIdent)
{
    if (mRenderSession)
        if (const auto texture = findSessionItem<Texture>(itemIdent))
            return mRenderSession->getTextureHandle(texture->id);
    return 0;
}

quint64 AppScriptObject::getBufferHandle(QJSValue itemIdent)
{
    if (mRenderSession)
        if (const auto buffer = findSessionItem<Buffer>(itemIdent))
            return mRenderSession->getBufferHandle(buffer->id);
    return 0;
}

QJSValue AppScriptObject::processShader(QJSValue itemIdent, QString processType)
{
    if (const auto shader = findSessionItem<Shader>(itemIdent)) {
        auto result = QVariant();
        auto renderer = Singletons::sessionRenderer();
        auto processSource = ProcessSource(renderer);
        connect(&processSource, &ProcessSource::outputChanged,
            [&](QVariant output) { result = output; });

        processSource.setFileName(shader->fileName);
        processSource.setSourceType(getSourceType(*shader));
        processSource.setProcessType(processType);
        processSource.update();

        // block until process source signaled completion
        while (processSource.updating())
            qApp->processEvents(QEventLoop::WaitForMoreEvents);

        return QJSValue(engine().toManagedValue(result));
    }
    return QJSValue::UndefinedValue;
}
