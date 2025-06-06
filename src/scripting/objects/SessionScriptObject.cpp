#include "SessionScriptObject.h"
#include "AppScriptObject.h"
#include "SynchronizeLogic.h"
#include "FileCache.h"
#include "Singletons.h"
#include "editors/EditorManager.h"
#include "editors/binary/BinaryEditor.h"
#include "editors/texture/TextureEditor.h"
#include "editors/source/SourceEditor.h"
#include "session/SessionModel.h"
#include "LibraryScriptObject.h"
#include "EditorScriptObject.h"
#include "../IScriptRenderSession.h"
#include "render/Renderer.h"
#include "render/ProcessSource.h"
#include <QFloat16>
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

    BinaryEditor *openBinaryEditor(const FileItem &buffer)
    {
        auto &editors = Singletons::editorManager();
        editors.setAutoRaise(false);
        auto editor = editors.openBinaryEditor(buffer.fileName);
        if (!editor)
            editor = editors.openNewBinaryEditor(buffer.fileName);
        editors.setAutoRaise(true);
        return editor;
    }

    TextureEditor *openTextureEditor(const FileItem &texture)
    {
        auto &editors = Singletons::editorManager();
        editors.setAutoRaise(false);
        auto editor = editors.openTextureEditor(texture.fileName);
        if (!editor)
            editor = editors.openNewTextureEditor(texture.fileName);
        editors.setAutoRaise(true);
        return editor;
    }

    SourceEditor *openSourceEditor(const FileItem &item)
    {
        auto &editors = Singletons::editorManager();
        editors.setAutoRaise(false);
        auto editor = editors.openSourceEditor(item.fileName);
        if (!editor)
            editor = editors.openNewSourceEditor(item.fileName);
        editors.setAutoRaise(true);
        return editor;
    }

    IEditor *openEditor(const FileItem &item)
    {
        switch (item.type) {
        case Item::Type::Buffer:  return openBinaryEditor(item);
        case Item::Type::Texture: return openTextureEditor(item);
        case Item::Type::Shader:
        case Item::Type::Script:  return openSourceEditor(item);
        default:                  return nullptr;
        }
    }
} // namespace

//-------------------------------------------------------------------------

SessionScriptObject_ItemObject::SessionScriptObject_ItemObject(
    SessionScriptObject *sessionObject, ItemId itemId)
    : mSessionObject(*sessionObject)
    , mItemId(itemId)
{
    auto &session = mSessionObject.threadSessionModel();
    auto index = session.getIndex(session.findItem(mItemId));
    Q_ASSERT(index.isValid());

    const auto json = session.getJson({ index }).first().toObject();
    for (auto it = json.begin(); it != json.end(); ++it)
        if (it.key() != "items")
            insert(it.key(), it.value().toVariant());

    setItemsList(session.getItem(index).items);
}

void SessionScriptObject_ItemObject::setItemsList(const QList<Item *> &items)
{
    insert("items", QVariant::fromValue(mSessionObject.makeArray([&](auto add) {
        for (const auto *item : items)
            add(mSessionObject.createItemObject(item->id));
    })));
}

QVariant SessionScriptObject_ItemObject::updateValue(const QString &key,
    const QVariant &input)
{
    auto update = QJsonObject();
    update.insert("id", mItemId);
    update.insert(key, input.toJsonValue());

    mSessionObject.withSessionModel(
        [itemId = mItemId, update](SessionModel &session) {
            const auto index = session.getIndex(session.findItem(itemId));
            if (index.isValid())
                session.dropJson({ update }, index.row(), index.parent(), true);
        });
    return input;
}

//-------------------------------------------------------------------------

SessionScriptObject::SessionScriptObject(AppScriptObject *appScriptObject)
    : QObject(appScriptObject)
    , mAppScriptObject(*appScriptObject)
{
}

void SessionScriptObject::initializeEngine(QJSEngine *engine)
{
    mEngine = engine;
}

SessionModel &SessionScriptObject::threadSessionModel()
{
    if (onMainThread()) {
        return Singletons::sessionModel();
    } else {
        Q_ASSERT(mRenderSession);
        return mRenderSession->sessionModelCopy();
    }
}

QJSEngine &SessionScriptObject::engine()
{
    Q_ASSERT(mEngine);
    return *mEngine;
}

void SessionScriptObject::setSelection(const QModelIndexList &selectedIndices)
{
    mSelectionProperty = makeArray([&](auto add) {
        for (const auto &index : selectedIndices)
            add(createItemObject(threadSessionModel().getItem(index).id));
    });
}

void SessionScriptObject::withSessionModel(UpdateFunction &&updateFunction)
{
    if (onMainThread()) {
        updateFunction(Singletons::sessionModel());
    } else {
        Q_ASSERT(mRenderSession);
        updateFunction(mRenderSession->sessionModelCopy());
        mPendingUpdates.push_back(std::move(updateFunction));
    }
}

void SessionScriptObject::beginBackgroundUpdate(
    IScriptRenderSession *renderSession)
{
    Q_ASSERT(!onMainThread());
    mRenderSession = renderSession;
}

void SessionScriptObject::endBackgroundUpdate()
{
    Q_ASSERT(onMainThread());
    mRenderSession = nullptr;

    auto &session = Singletons::sessionModel();
    if (!mPendingUpdates.empty()) {
        session.beginUndoMacro("Script");
        for (const auto &update : mPendingUpdates)
            update(session);
        mPendingUpdates.clear();
        session.endUndoMacro();
    }

    Singletons::fileCache().updateFromEditors();
}

bool SessionScriptObject::available() const
{
    return (onMainThread() || mRenderSession != nullptr);
}

int SessionScriptObject::id()
{
    return threadSessionModel().sessionItem().id;
}

QString SessionScriptObject::name()
{
    return threadSessionModel().sessionItem().name;
}

void SessionScriptObject::setName(QString name)
{
    threadSessionModel().setData(
        threadSessionModel().sessionItemIndex(SessionModel::Name), name);
}

QJSValue SessionScriptObject::items()
{
    if (mSessionItems.isUndefined())
        mSessionItems = makeArray([&](auto add) {
            for (const auto *item : threadSessionModel().sessionItem().items)
                add(createItemObject(item->id));
        });
    return mSessionItems;
}

QJSValue SessionScriptObject::insertItem(QJSValue object)
{
    return insertItem(QJSValue::UndefinedValue, object);
}

QJsonObject SessionScriptObject::toJsonObject(const QJSValue &value)
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

QJSValue SessionScriptObject::insertItemAt(const Item *parent, int row,
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
    refreshItemObjectItems(parent);

    return createItemObject(id);
}

QJSValue SessionScriptObject::insertItem(QJSValue parentIdent, QJSValue object)
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

QJSValue SessionScriptObject::insertBeforeItem(QJSValue siblingIdent,
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

QJSValue SessionScriptObject::insertAfterItem(QJSValue siblingIdent,
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

void SessionScriptObject::replaceItems(QJSValue parentIdent, QJSValue array)
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

    refreshItemObjectItems(parent);
}

const Item *SessionScriptObject::findSessionItem(QJSValue itemIdent,
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
                if (itemIdent.call({ createItemObject(item.id) }).toBool())
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

const Item *SessionScriptObject::findSessionItem(QJSValue itemIdent,
    QJSValue originIdent, bool searchSubItems)
{
    const auto originItem = findSessionItem(originIdent);
    if (!originItem) {
        engine().throwError(QStringLiteral("Invalid origin"));
        return nullptr;
    }
    const auto originIndex = threadSessionModel().getIndex(originItem);
    return findSessionItem(itemIdent, originIndex, searchSubItems);
}

const Item *SessionScriptObject::findSessionItem(QJSValue itemIdent)
{
    return findSessionItem(itemIdent, threadSessionModel().sessionItemIndex(),
        true);
}

QJSValue SessionScriptObject::createItemObject(ItemId itemId)
{
    Q_ASSERT(itemId);
    auto &[object, jsValue] = mCreatedItemObjects[itemId];
    if (!object) {
        object = new ItemObject(this, itemId);
        jsValue = engine().newQObject(object);
    }
    return jsValue;
}

void SessionScriptObject::refreshItemObjectItems(const Item *item)
{
    if (!item)
        return;

    const auto it = mCreatedItemObjects.find(item->id);
    if (it != mCreatedItemObjects.end())
        it->second.first->setItemsList(item->items);
}

QJSValue SessionScriptObject::getParentItem(QJSValue itemIdent)
{
    const auto item = findSessionItem(itemIdent);
    if (!item || !item->parent)
        return QJSValue::UndefinedValue;

    return createItemObject(item->parent->id);
}

QJSValue SessionScriptObject::findItem(QJSValue itemIdent, QJSValue originIdent,
    bool searchSubItems)
{
    if (auto item = findSessionItem(itemIdent, originIdent, searchSubItems))
        return createItemObject(item->id);
    return QJSValue::UndefinedValue;
}

QJSValue SessionScriptObject::findItem(QJSValue itemIdent)
{
    if (auto item = findSessionItem(itemIdent))
        return createItemObject(item->id);
    return QJSValue::UndefinedValue;
}

QJSValue SessionScriptObject::findItems(QJSValue itemIdent,
    QJSValue originIdent, bool searchSubItems)
{
    return makeArray([&](auto add) {
        if (itemIdent.isCallable()) {
            const auto &session = threadSessionModel();
            auto originIndex = session.sessionItemIndex();
            if (!originIdent.isUndefined()) {
                const auto origin = findSessionItem(originIdent);
                if (!origin) {
                    engine().throwError(QStringLiteral("Invalid origin"));
                    return;
                }
                originIndex = session.getIndex(origin);
            }

            const auto callback = [&](const Item &item) {
                if (item.type != Item::Type::Root) {
                    auto itemObject = createItemObject(item.id);
                    if (itemIdent.call({ itemObject }).toBool())
                        add(itemObject);
                }
            };
            if (searchSubItems) {
                session.forEachItem(originIndex, callback);
            } else {
                session.forEachItemDirect(originIndex, callback);
            }
        } else {
            if (auto item =
                    findSessionItem(itemIdent, originIdent, searchSubItems))
                add(createItemObject(item->id));
        }
    });
}

QJSValue SessionScriptObject::findItems(QJSValue itemIdent)
{
    return findItems(itemIdent, QJSValue::UndefinedValue, true);
}

void SessionScriptObject::clearItems(QJSValue parentIdent)
{
    const auto parent = findSessionItem(parentIdent);
    if (!parent)
        return engine().throwError(QStringLiteral("Invalid parent"));

    withSessionModel([parentId = parent->id](SessionModel &session) {
        const auto index = session.getIndex(session.findItem(parentId));
        if (index.isValid())
            session.removeRows(0, session.rowCount(index), index);
    });
    refreshItemObjectItems(parent);
}

void SessionScriptObject::deleteItem(QJSValue itemIdent)
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
    refreshItemObjectItems(parent);
}

QJSValue SessionScriptObject::openEditor(QJSValue itemIdent)
{
    const auto item = findSessionItem<FileItem>(itemIdent);
    if (!item)
        return QJSValue::UndefinedValue;

    const auto fileName = std::make_shared<QString>();
    withSessionModel(
        [itemId = item->id, fileName](SessionModel &session) mutable {
            if (const auto item = session.findItem<FileItem>(itemId)) {
                ensureFileName(session, *item, fileName.get());

                if (onMainThread())
                    ::openEditor(*item);
            }
        });
    if (fileName->isEmpty())
        return {};
    return mAppScriptObject.openEditor(*fileName);
}

void SessionScriptObject::setBufferData(QJSValue itemIdent, QJSValue data)
{
    const auto buffer = findSessionItem<Buffer>(itemIdent);
    if (!buffer)
        return engine().throwError(QStringLiteral("Invalid item"));

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
                if (auto editor = openBinaryEditor(*buffer))
                    editor->replace(bufferData);
        }
    });
}

void SessionScriptObject::setBlockData(QJSValue itemIdent, QJSValue data)
{
    const auto block = findSessionItem<Block>(itemIdent);
    if (!block)
        return engine().throwError(QStringLiteral("Invalid item"));

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
                if (onMainThread())
                    if (auto editor = openBinaryEditor(*buffer)) {
                        auto offset = 0, rowCount = 0;
                        Singletons::synchronizeLogic().evaluateBlockProperties(
                            *block, &offset, &rowCount);
                        editor->replaceRange(offset, bufferData);
                    }
            }
    });
}

void SessionScriptObject::setTextureData(QJSValue itemIdent, QJSValue data)
{
    const auto texture = findSessionItem<Texture>(itemIdent);
    if (!texture)
        return engine().throwError(QStringLiteral("Invalid item"));

    const auto textureData = toTextureData(data, *texture);
    if (textureData.isNull())
        return engine().throwError(QStringLiteral("Invalid data"));

    withSessionModel([textureId = texture->id, textureData,
                         fileName = QString()](SessionModel &session) mutable {
        if (auto texture = session.findItem<Texture>(textureId)) {
            ensureFileName(session, *texture, &fileName);
            if (onMainThread())
                if (auto editor = openTextureEditor(*texture))
                    editor->replace(textureData);
        }
    });
}

void SessionScriptObject::setShaderSource(QJSValue itemIdent, QJSValue data)
{
    const auto shader = findSessionItem<Shader>(itemIdent);
    if (!shader)
        return engine().throwError(QStringLiteral("Invalid item"));

    withSessionModel([shaderId = shader->id, data = data.toString(),
                         fileName = QString()](SessionModel &session) mutable {
        if (auto shader = session.findItem<Shader>(shaderId)) {
            ensureFileName(session, *shader, &fileName);
            if (onMainThread())
                if (auto editor = openSourceEditor(*shader))
                    editor->replace(data);
        }
    });
}

void SessionScriptObject::setScriptSource(QJSValue itemIdent, QJSValue data)
{
    const auto script = findSessionItem<Script>(itemIdent);
    if (!script)
        return engine().throwError(QStringLiteral("Invalid item"));

    withSessionModel([scriptId = script->id, data = data.toString(),
                         fileName = QString()](SessionModel &session) mutable {
        if (auto script = session.findItem<Script>(scriptId)) {
            ensureFileName(session, *script, &fileName);
            if (onMainThread())
                if (auto editor = openSourceEditor(*script))
                    editor->replace(data);
        }
    });
}

quint64 SessionScriptObject::getTextureHandle(QJSValue itemIdent)
{
    if (mRenderSession)
        if (const auto texture = findSessionItem<Texture>(itemIdent))
            return mRenderSession->getTextureHandle(texture->id);
    return 0;
}

quint64 SessionScriptObject::getBufferHandle(QJSValue itemIdent)
{
    if (mRenderSession)
        if (const auto buffer = findSessionItem<Buffer>(itemIdent))
            return mRenderSession->getBufferHandle(buffer->id);
    return 0;
}

QJSValue SessionScriptObject::processShader(QJSValue itemIdent,
    QString processType)
{
    if (const auto shader = findSessionItem<Shader>(itemIdent)) {
        auto result = QVariant();
        auto renderer = Singletons::sessionRenderer();
        auto processSource = ProcessSource(renderer);
        connect(&processSource, &ProcessSource::outputChanged,
            [&](QVariant output) { result = output; });

        processSource.setFileName(shader->fileName);
        processSource.setSourceType(
            getSourceType(shader->shaderType, shader->language));
        processSource.setProcessType(processType);
        processSource.update();

        // block until process source signaled completion
        while (processSource.updating())
            qApp->processEvents(QEventLoop::WaitForMoreEvents);

        return QJSValue(mEngine->toManagedValue(result));
    }
    return QJSValue::UndefinedValue;
}
