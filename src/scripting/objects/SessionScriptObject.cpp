#include "SessionScriptObject.h"
#include "EvaluatedPropertyCache.h"
#include "FileCache.h"
#include "Singletons.h"
#include "editors/EditorManager.h"
#include "editors/binary/BinaryEditor.h"
#include "editors/texture/TextureEditor.h"
#include "editors/source/SourceEditor.h"
#include "session/SessionModel.h"
#include "LibraryScriptObject.h"
#include "../ScriptEngine.h"
#include "../IScriptRenderSession.h"
#include <QFloat16>
#include <QJSEngine>
#include <QTextStream>
#include <cstring>

namespace {
    QByteArray toByteArray(const QJSValue &data, const Block &block,
        MessagePtrSet &messages)
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
                        pos += padding[j];
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
        } else {
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
                        ADD(Field::DataType::Int64, int64_t, toInt)
                        ADD(Field::DataType::Uint64, uint64_t, toUInt)
                        ADD(Field::DataType::Float, float, toNumber)
                        ADD(Field::DataType::Double, double, toNumber)
#undef ADD
                    }
                    pos += padding[j];
                }
            }
        }
        return bytes;
    } // namespace

    TextureData toTextureData(const QJSValue &data, const Texture &texture,
        MessagePtrSet &messages)
    {
        auto textureData = TextureData();
        if (!textureData.create(texture.target, texture.format,
                texture.width.toInt(), texture.height.toInt(),
                texture.depth.toInt(), texture.layers.toInt()))
            return {};

        const auto components = getTextureComponentCount(texture.format);
        const auto count = (textureData.width() * textureData.height()
            * textureData.depth() * components);
        if (data.property("length").toInt() != count)
            return {};

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

    void ensureFileName(SessionModel &session, const FileItem *item)
    {
        if (item->fileName.isEmpty())
            session.setData(session.getIndex(item, SessionModel::FileName),
                FileDialog::generateNextUntitledFileName(item->name));
    }

    BinaryEditor *openBinaryEditor(const Buffer &buffer)
    {
        auto &editors = Singletons::editorManager();
        editors.setAutoRaise(false);
        auto editor = editors.openBinaryEditor(buffer.fileName);
        if (!editor)
            editor = editors.openNewBinaryEditor(buffer.fileName);
        editors.setAutoRaise(true);
        return editor;
    }

    TextureEditor *openTextureEditor(const Texture &texture)
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

    bool canHaveItems(Item::Type type)
    {
        switch (type) {
        case Item::Type::Session:
        case Item::Type::Group:
        case Item::Type::Buffer:
        case Item::Type::Block:
        case Item::Type::Program:
        case Item::Type::Stream:
        case Item::Type::Target:  return true;
        default:                  return false;
        }
    }
} // namespace

//-------------------------------------------------------------------------

SessionScriptObject_ItemObject::SessionScriptObject_ItemObject(
    SessionScriptObject *sessionObject, ItemId itemId)
    : mSessionObject(*sessionObject)
    , mItemId(itemId)
{
    auto &session = sessionObject->threadSessionModel();
    auto index = session.getIndex(session.findItem(mItemId));
    Q_ASSERT(index.isValid());

    const auto json = session.getJson({ index }).first().toObject();
    for (auto it = json.begin(); it != json.end(); ++it)
        if (it.key() != "items")
            insert(it.key(), it.value().toVariant());

    if (canHaveItems(session.getItemType(index))) {
        auto items = mSessionObject.engine().newArray();
        auto i = 0;
        for (auto subItem : session.getItem(index).items) {
            items.setProperty(i++,
                mSessionObject.engine().newQObject(
                    new SessionScriptObject_ItemObject(&mSessionObject,
                        subItem->id)));
        }
        insert("items", QVariant::fromValue(items));
    }
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

SessionScriptObject::SessionScriptObject(QObject *parent) : QObject(parent) { }

void SessionScriptObject::initializeEngine(QJSEngine *engine)
{
    mEngine = engine;
}

SessionModel &SessionScriptObject::threadSessionModel()
{
    if (mSessionCopy) {
        Q_ASSERT(!onMainThread());
        return *mSessionCopy;
    } else {
        Q_ASSERT(onMainThread());
        return Singletons::sessionModel();
    }
}

QJSEngine &SessionScriptObject::engine()
{
    Q_ASSERT(mEngine);
    return *mEngine;
}

void SessionScriptObject::withSessionModel(UpdateFunction &&updateFunction)
{
    if (onMainThread()) {
        Q_ASSERT(!mSessionCopy);
        updateFunction(Singletons::sessionModel());
    } else {
        Q_ASSERT(mSessionCopy);
        updateFunction(*mSessionCopy);
        mPendingUpdates.push_back(std::move(updateFunction));
    }
}

void SessionScriptObject::beginBackgroundUpdate(SessionModel *sessionCopy,
    IScriptRenderSession *renderSession)
{
    Q_ASSERT(!onMainThread());
    mSessionCopy = sessionCopy;
    mRenderSession = renderSession;
}

void SessionScriptObject::endBackgroundUpdate()
{
    Q_ASSERT(onMainThread());
    mSessionCopy = nullptr;

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

QString SessionScriptObject::sessionName()
{
    return threadSessionModel().sessionItem().name;
}

void SessionScriptObject::setSessionName(QString name)
{
    threadSessionModel().setData(
        threadSessionModel().sessionItemIndex(SessionModel::Name), name);
}

QJSValue SessionScriptObject::sessionItems()
{
    if (mSessionItems.isUndefined()) {
        mSessionItems = engine().newArray();
        auto i = 0;
        for (auto item : threadSessionModel().sessionItem().items)
            mSessionItems.setProperty(i++,
                engine().newQObject(new ItemObject(this, item->id)));
    }
    return mSessionItems;
}

ItemId SessionScriptObject::getItemId(const QJSValue &itemDesc)
{
    if (itemDesc.isObject())
        return itemDesc.property("id").toInt();

    if (itemDesc.isString()) {
        auto &session = threadSessionModel();
        const auto parts = itemDesc.toString().split('/');
        auto index = session.sessionItemIndex();
        for (const auto &part : parts) {
            index = session.findChildByName(index, part);
            if (!index.isValid())
                return 0;
        }
        return session.getItemId(index);
    }
    return itemDesc.toInt();
}

QJSValue SessionScriptObject::getItem(QModelIndex index)
{
    Q_ASSERT(index.isValid());
    auto &session = threadSessionModel();
    auto item = session.getItem(index);
    return engine().newQObject(new ItemObject(this, item.id));
}

QJSValue SessionScriptObject::insertItem(QJSValue object)
{
    auto &session = threadSessionModel();
    return insertItem(session.sessionItem().id, object);
}

QJsonObject SessionScriptObject::toJsonObject(const QJSValue &value)
{
    if (auto object = value.toQObject()) {
        // create JSON object from ItemObject
        if (const auto item = qobject_cast<const QQmlPropertyMap *>(object)) {
            auto jsonObject = QJsonObject();
            for (const auto &key : item->keys())
                jsonObject.insert(key,
                    QJsonValue::fromVariant(item->value(key)));
            return jsonObject;
        }
        return {};
    }
    return value.toVariant().toJsonObject();
}

QJSValue SessionScriptObject::insertItem(QJSValue itemDesc, QJSValue object)
{
    const auto parent = getItem(itemDesc);
    if (!parent) {
        engine().throwError(QStringLiteral("Invalid parent"));
        return QJSValue::UndefinedValue;
    }

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

    withSessionModel([parentId = parent->id, update](SessionModel &session) {
        const auto parent = session.getIndex(session.findItem(parentId),
            SessionModel::ColumnType::Name);
        session.dropJson({ update }, session.rowCount(parent), parent, true);
    });

    return engine().newQObject(new ItemObject(this, id));
}

void SessionScriptObject::replaceItems(QJSValue itemDesc, QJSValue array)
{
    const auto count = array.property("length").toInt();
    auto update = QJsonArray();
    for (auto i = 0; i < count; ++i) {
        const auto object = toJsonObject(array.property(i));
        if (object.isEmpty()) {
            engine().throwError(QStringLiteral("Invalid object"));
            return;
        }
        update.append(object);
    }

    const auto parent = getItem(itemDesc);
    if (!parent)
        return;

    // collect IDs in current list
    auto unusedIds = std::vector<std::pair<ItemId, Item::Type>>();
    for (const auto &item : parent->items)
        unusedIds.emplace_back(item->id, item->type);

    // remove IDs contained in new list
    for (auto i = 0; i < update.size(); ++i)
        if (const auto id = update[i].toObject()["id"].toInt()) {
            const auto it = std::find_if(unusedIds.begin(), unusedIds.end(),
                [&](const auto &pair) { return pair.first == id; });
            if (it != unusedIds.end())
                unusedIds.erase(it);
        }

    // reuse IDs of items with same type or assign a new one
    for (auto i = 0; i < update.size(); ++i) {
        auto object = update[i].toObject();
        auto id = object["id"].toInt();
        if (!id) {
            const auto type = getItemTypeByName(object["type"].toString());
            const auto it = std::find_if(unusedIds.begin(), unusedIds.end(),
                [&](const auto &pair) { return pair.second == type; });
            if (it != unusedIds.end()) {
                id = it->first;
                unusedIds.erase(it);
            } else {
                id = threadSessionModel().getNextItemId();
            }
            object["id"] = id;
            update[i] = object;
            
            // update IDs inplace
            array.property(i).setProperty("id", id);
        }
    }

    withSessionModel(
        [parentId = parent->id, update, unusedIds](SessionModel &session) {
            const auto parent = session.getIndex(session.findItem(parentId),
                SessionModel::ColumnType::Name);
            session.dropJson(update, session.rowCount(parent), parent, true);
            for (const auto &[id, type] : unusedIds)
                session.deleteItem(session.getIndex(session.findItem(id)));
        });
}

const Item *SessionScriptObject::getItem(const QJSValue &itemDesc)
{
    const auto itemId = getItemId(itemDesc);
    if (!itemId)
        return nullptr;
    auto &session = threadSessionModel();
    return session.findItem(itemId);
}

QJSValue SessionScriptObject::item(QJSValue itemDesc)
{
    const auto item = getItem(itemDesc);
    if (!item)
        return QJSValue::UndefinedValue;

    return engine().newQObject(new ItemObject(this, item->id));
}

void SessionScriptObject::clear()
{
    withSessionModel([](SessionModel &session) { session.clear(); });
}

void SessionScriptObject::clearItem(QJSValue itemDesc)
{
    if (const auto item = getItem(itemDesc))
        withSessionModel([itemId = item->id](SessionModel &session) {
            const auto index = session.getIndex(session.findItem(itemId));
            if (index.isValid())
                session.removeRows(0, session.rowCount(index), index);
        });
}

void SessionScriptObject::deleteItem(QJSValue itemDesc)
{
    if (const auto item = getItem(itemDesc))
        withSessionModel([itemId = item->id](SessionModel &session) {
            const auto index = session.getIndex(session.findItem(itemId));
            if (index.isValid())
                session.deleteItem(index);
        });
}

void SessionScriptObject::setBufferData(QJSValue itemDesc, QJSValue data)
{
    if (const auto buffer = getItem<Buffer>(itemDesc)) {
        auto block = castItem<Block>(buffer->items[0]);
        if (!block || block->items.empty()) {
            engine().throwError(QStringLiteral("Buffer structure not defined"));
            return;
        }

        withSessionModel([this, bufferId = buffer->id,
                             data = toByteArray(data, *block, mMessages)](
                             SessionModel &session) {
            if (auto buffer = session.findItem<Buffer>(bufferId)) {
                ensureFileName(session, buffer);
                if (onMainThread())
                    if (auto editor = openBinaryEditor(*buffer))
                        editor->replace(data);
            }
        });
    }
}

void SessionScriptObject::setBlockData(QJSValue itemDesc, QJSValue data)
{
    if (const auto block = getItem<Block>(itemDesc)) {
        Singletons::evaluatedPropertyCache().invalidate(block->id);
        withSessionModel([this, blockId = block->id,
                             data = toByteArray(data, *block, mMessages)](
                             SessionModel &session) {
            if (auto block = session.findItem<Block>(blockId))
                if (auto buffer = castItem<Buffer>(block->parent)) {
                    ensureFileName(session, castItem<Buffer>(block->parent));
                    if (onMainThread())
                        if (auto editor = openBinaryEditor(*buffer)) {
                            auto offset = 0, rowCount = 0;
                            Singletons::evaluatedPropertyCache()
                                .evaluateBlockProperties(*block, &offset,
                                    &rowCount);
                            editor->replaceRange(offset, data);
                        }
                }
        });
    }
}

void SessionScriptObject::setTextureData(QJSValue itemDesc, QJSValue data)
{
    if (const auto texture = getItem<Texture>(itemDesc))
        withSessionModel([this, textureId = texture->id,
                             data = toTextureData(data, *texture, mMessages)](
                             SessionModel &session) {
            if (auto texture = session.findItem<Texture>(textureId)) {
                ensureFileName(session, texture);
                if (onMainThread())
                    if (auto editor = openTextureEditor(*texture))
                        editor->replace(data);
            }
        });
}

void SessionScriptObject::setShaderSource(QJSValue itemDesc, QJSValue data)
{
    if (const auto shader = getItem<Shader>(itemDesc))
        withSessionModel([this, shaderId = shader->id,
                             data = data.toString()](SessionModel &session) {
            if (auto shader = session.findItem<Shader>(shaderId)) {
                ensureFileName(session, shader);
                if (onMainThread())
                    if (auto editor = openSourceEditor(*shader))
                        editor->replace(data);
            }
        });
}

void SessionScriptObject::setScriptSource(QJSValue itemDesc, QJSValue data)
{
    if (const auto script = getItem<Script>(itemDesc))
        withSessionModel([this, scriptId = script->id,
                             data = data.toString()](SessionModel &session) {
            if (auto script = session.findItem<Script>(scriptId)) {
                ensureFileName(session, script);
                if (onMainThread())
                    if (auto editor = openSourceEditor(*script))
                        editor->replace(data);
            }
        });
}

quint64 SessionScriptObject::getTextureHandle(QJSValue itemDesc)
{
    if (mRenderSession)
        if (const auto texture = getItem<Texture>(itemDesc))
            return mRenderSession->getTextureHandle(texture->id);
    return 0;
}
