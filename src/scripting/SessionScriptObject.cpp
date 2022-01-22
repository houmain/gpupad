#include "SessionScriptObject.h"
#include "ScriptEngine.h"
#include "EvaluatedPropertyCache.h"
#include "editors/EditorManager.h"
#include "editors/TextureEditor.h"
#include "editors/BinaryEditor.h"
#include "session/SessionModel.h"
#include "FileCache.h"
#include "Singletons.h"
#include <QQmlPropertyMap>
#include <QTextStream>
#include <QFloat16>
#include <cstring>

namespace
{
    // there does not seems to be a way to access e.g. Float32Array directly...
    QByteArray toByteArray(const QJSValue &data, const Block &block)
    {
        auto columns = QList<const Field*>();
        for (auto column : block.items)
            columns.append(static_cast<const Field*>(column));
        auto elementsPerRow = 0;
        for (const auto &column : columns)
            elementsPerRow += column->count;
        if (!elementsPerRow)
            return { };

        const auto rowCount = data.property("length").toInt() / elementsPerRow;
        auto bytes = QByteArray();
        bytes.resize(getBlockStride(block) * rowCount);

        auto pos = bytes.data();
        const auto write = [&](auto v) {
            std::memcpy(pos, &v, sizeof(v));
            pos += sizeof(v);
        };

        auto index = 0;
        for (auto i = 0; i < rowCount; ++i) {
            const auto &column = columns[i % columns.size()];
            for (auto j = 0; j < elementsPerRow; ++j, ++index) {
                const auto value = data.property(index);
                switch (column->dataType) {
                    case Field::DataType::Int8: write(static_cast<int8_t>(value.toInt())); break;
                    case Field::DataType::Int16: write(static_cast<int16_t>(value.toInt())); break;
                    case Field::DataType::Int32: write(static_cast<int32_t>(value.toInt())); break;
                    case Field::DataType::Uint8: write(static_cast<uint8_t>(value.toUInt())); break;
                    case Field::DataType::Uint16: write(static_cast<uint16_t>(value.toUInt())); break;
                    case Field::DataType::Uint32: write(static_cast<uint32_t>(value.toUInt())); break;
                    case Field::DataType::Float: write(static_cast<float>(value.toNumber())); break;
                    case Field::DataType::Double: write(static_cast<double>(value.toNumber())); break;
                }
            }
            pos += column->padding;
        }
        return bytes;
    }

    TextureData toTextureData(const QJSValue &data, const Texture &texture)
    {
        auto textureData = TextureData();
        if (!textureData.create(texture.target, texture.format,
                texture.width.toInt(), texture.height.toInt(), texture.depth.toInt(),
                texture.layers.toInt(), texture.samples))
            return { };

        const auto components = getTextureComponentCount(texture.format);
        const auto count = (textureData.width() *
            textureData.height() * textureData.depth() * components);
        if (data.property("length").toInt() != count)
            return { };

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

                default:
                    return { };
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

    bool canHaveItems(Item::Type type) 
    {
        switch (type) {
            case Item::Type::Group:
            case Item::Type::Buffer:
            case Item::Type::Block:
            case Item::Type::Program:
            case Item::Type::Stream:
            case Item::Type::Target:
                return true;
            default:
                return false;
        }
    }
} // namespace

//-------------------------------------------------------------------------

class SessionScriptObject::ItemObject : public QQmlPropertyMap
{
private:
    SessionScriptObject &mSessionObject;
    ItemId mItemId;

    QVariant updateValue(const QString &key, const QVariant &input) override;

public:
    ItemObject(SessionScriptObject *sessionObject, ItemId itemId);
};

class SessionScriptObject::ItemListObject : public QQmlPropertyMap
{
private:
    SessionScriptObject &mSessionObject;
    ItemId mParentItemId;

    QVariant updateValue(const QString &key, const QVariant &input) override;

public:
    ItemListObject(SessionScriptObject *sessionObject, ItemId parentItemId);
};

//-------------------------------------------------------------------------

SessionScriptObject::ItemObject::ItemObject(SessionScriptObject *sessionObject, ItemId itemId)
    : mSessionObject(*sessionObject)
    , mItemId(itemId)
{
    auto& session = mSessionObject.threadSessionModel();
    const auto index = session.getIndex(session.findItem(mItemId));
    if (index.isValid()) {
        const auto json = session.getJson({ index }).first().toObject();
        for (auto it = json.begin(); it != json.end(); ++it)
            insert(it.key(), it.value().toVariant());

        if (canHaveItems(session.getItemType(index)))
            insert("items", QVariant::fromValue(new ItemListObject(&mSessionObject, mItemId)));
    }
}

QVariant SessionScriptObject::ItemObject::updateValue(const QString &key, const QVariant &input)
{
    auto update = QJsonObject();
    update.insert("id", mItemId);
    update.insert(key, input.toJsonValue());

    mSessionObject.withSessionModel([itemId = mItemId, update](SessionModel &session){
        const auto index = session.getIndex(session.findItem(itemId));
        if (index.isValid())
            session.dropJson({ update }, index.row(), index.parent(), true);
    });
    return input;
}

//-------------------------------------------------------------------------

SessionScriptObject::ItemListObject::ItemListObject(SessionScriptObject *sessionObject, ItemId parentItemId)
    : mSessionObject(*sessionObject)
    , mParentItemId(parentItemId)
{
    auto& session = mSessionObject.threadSessionModel();
    const auto parent = session.getIndex(session.findItem(mParentItemId));
    for (auto row = 0; row < session.rowCount(parent); ++row) {
        auto index = session.index(row, 0, parent);
        auto id = session.getItemId(index);
        auto name = session.data(index, SessionModel::Name).toString();
        auto item = QVariant::fromValue(new ItemObject(&mSessionObject, id));
        insert(name, item);
    }
}

QVariant SessionScriptObject::ItemListObject::updateValue(const QString &key, const QVariant &input)
{
    auto update = mSessionObject.engine().toScriptValue(input).toVariant().toJsonObject();
    const auto id = (update.contains("id") ? update["id"].toInt() :
        mSessionObject.threadSessionModel().getNextItemId());
    update.insert("id", id);
    update.insert("name", key);

    mSessionObject.withSessionModel([parentItemId = mParentItemId, update](SessionModel &session){
        const auto parent = session.getIndex(session.findItem(parentItemId), SessionModel::ColumnType::Name);
        session.dropJson({ update }, session.rowCount(parent), parent, true);
    });

    return QVariant::fromValue(new ItemObject(&mSessionObject, id));
}

//-------------------------------------------------------------------------

SessionScriptObject::SessionScriptObject(QJSEngine *engine)
    : QObject(engine)
    , mEngine(engine)
{
}

SessionModel &SessionScriptObject::threadSessionModel()
{
    if (mSessionCopy) {
        Q_ASSERT(!onMainThread());
        return *mSessionCopy;
    }
    else {
        Q_ASSERT(onMainThread());
        return Singletons::sessionModel();
    }
}

QJSEngine &SessionScriptObject::engine()
{
    return *mEngine;
}

void SessionScriptObject::withSessionModel(UpdateFunction &&updateFunction)
{
    if (onMainThread()) {
        Q_ASSERT(!mSessionCopy);
        updateFunction(Singletons::sessionModel());
    }
    else {
        Q_ASSERT(mSessionCopy);
        updateFunction(*mSessionCopy);
        mPendingUpdates.push_back(std::move(updateFunction));
    }
}

void SessionScriptObject::beginBackgroundUpdate(SessionModel *sessionCopy)
{
    Q_ASSERT(!onMainThread());
    mSessionCopy = sessionCopy;
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

        if (mUpdatedEditor)
            Singletons::fileCache().updateEditorFiles();

        mUpdatedEditor = false;
    }
}

QJSValue SessionScriptObject::rootItems()
{
    if (mRootItemsList.isUndefined())
        mRootItemsList = engine().newQObject(new ItemListObject(this, 0));
    return mRootItemsList;
}

ItemId SessionScriptObject::getItemId(QJSValue itemDesc)
{
    if (itemDesc.isObject())
        return itemDesc.property("id").toInt();

    if (itemDesc.isString()) {
        auto &session = threadSessionModel();
        const auto parts = itemDesc.toString().split('/');
        auto index = QModelIndex();
        for (const auto &part : parts) {
            index = session.findChildByName(index, part);
            if (!index.isValid())
                return 0;
        }
        return session.getItemId(index);
    }
    return itemDesc.toInt();
}

QJSValue SessionScriptObject::item(QJSValue itemDesc)
{
    return engine().newQObject(new ItemObject(this, getItemId(itemDesc)));
}

void SessionScriptObject::clear()
{
    withSessionModel([](SessionModel &session) {
        session.clear();
    });
}

void SessionScriptObject::clearItem(QJSValue itemDesc)
{
    withSessionModel([itemId = getItemId(itemDesc)](SessionModel &session) {
        const auto index = session.getIndex(session.findItem(itemId));
        if (index.isValid())
            session.removeRows(0, session.rowCount(index), index);
    });
}

void SessionScriptObject::deleteItem(QJSValue itemDesc)
{
    withSessionModel([itemId = getItemId(itemDesc)](SessionModel &session) {
        const auto index = session.getIndex(session.findItem(itemId));
        if (index.isValid())
            session.deleteItem(index);
    });
}

void SessionScriptObject::setBufferData(QJSValue itemDesc, QJSValue data)
{
    if (auto buffer = threadSessionModel().findItem<Buffer>(getItemId(itemDesc)))
        if (!buffer->items.isEmpty())
            if (auto block = castItem<Block>(buffer->items[0]))
                return withSessionModel(
                    [this, bufferId = buffer->id, data = toByteArray(data, *block)](SessionModel &session) {
                        if (auto buffer = session.findItem<Buffer>(bufferId)) {
                            ensureFileName(session, buffer);
                            if (onMainThread())
                                if (auto editor = openBinaryEditor(*buffer)) {
                                    editor->replace(data, false);
                                    mUpdatedEditor = true;
                                }
                        }
                    });

    mMessages += MessageList::insert(0,
        MessageType::ScriptError, "setBufferData failed");
}

void SessionScriptObject::setBlockData(QJSValue itemDesc, QJSValue data)
{
    if (auto block = threadSessionModel().findItem<Block>(getItemId(itemDesc))) {
        Singletons::evaluatedPropertyCache().invalidate(block->id);
        return withSessionModel(
            [this, blockId = block->id, data = toByteArray(data, *block)](SessionModel &session) {
                if (auto block = session.findItem<Block>(blockId))
                    if (auto buffer = castItem<Buffer>(block->parent)) {
                        ensureFileName(session, castItem<Buffer>(block->parent));
                        if (onMainThread())
                            if (auto editor = openBinaryEditor(*buffer)){
                                auto offset = 0, rowCount = 0;
                                Singletons::evaluatedPropertyCache().evaluateBlockProperties(
                                    *block, &offset, &rowCount);
                                editor->replaceRange(offset, data, false);
                                mUpdatedEditor = true;
                            }
                    }
            });
    }

    mMessages += MessageList::insert(0,
        MessageType::ScriptError, "setBlockData failed");
}

void SessionScriptObject::setTextureData(QJSValue itemDesc, QJSValue data)
{
    if (auto texture = threadSessionModel().findItem<Texture>(getItemId(itemDesc)))
        return withSessionModel(
            [this, textureId = texture->id, data = toTextureData(data, *texture)](SessionModel &session) {
                if (auto texture = session.findItem<Texture>(textureId)) {
                    ensureFileName(session, texture);
                    if (onMainThread())
                        if (auto editor = openTextureEditor(*texture)) {
                            editor->replace(data, false);
                            mUpdatedEditor = true;
                        }
                }
            });

    mMessages += MessageList::insert(0,
        MessageType::ScriptError, "setTextureData failed");
}
