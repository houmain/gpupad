#include "SessionScriptObject.h"
#include "ScriptEngine.h"
#include "editors/EditorManager.h"
#include "editors/BinaryEditor.h"
#include "session/SessionModel.h"
#include "FileCache.h"
#include "Singletons.h"
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

    QString ensureFileName(SessionModel &session, const FileItem *item)
    {
        if (item->fileName.isEmpty())
            session.setData(session.getIndex(item, SessionModel::FileName),
                FileDialog::generateNextUntitledFileName(item->name));
        return item->fileName;
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
} // namespace

SessionScriptObject::SessionScriptObject(QObject *parent)
    : QObject{parent}
{
}

void SessionScriptObject::beginUpdate(ScriptEngine *scriptEngine, SessionModel *sessionCopy)
{
    mScriptEngine = scriptEngine;
    mSession = sessionCopy;

    if (scriptEngine->getGlobal("Session").isUndefined()) {
        scriptEngine->setGlobal("Session", this);
        auto file = QFile(":/scripting/SessionScriptObject.js");
        file.open(QFile::ReadOnly | QFile::Text);
        scriptEngine->evaluateScript(QTextStream(&file).readAll(), "", mMessages);
    }
}

void SessionScriptObject::endUpdate()
{
    mScriptEngine->evaluateScript("Session.updateItems()", "", mMessages);
    mSession = nullptr;
    mScriptEngine = nullptr;
}

void SessionScriptObject::applyUpdate()
{
    mSession = &Singletons::sessionModel();

    if (!mPendingUpdates.empty()) {
        mSession->beginUndoMacro("Script");
        for (const auto &update : mPendingUpdates)
            update();
        mPendingUpdates.clear();
        mSession->endUndoMacro();
    }

    if (!mPendingEditorUpdates.empty()) {
        for (const auto &update : mPendingEditorUpdates)
            update();
        mPendingEditorUpdates.clear();
        Singletons::fileCache().updateEditorFiles();
    }

    mSession = nullptr;
}

QJsonArray SessionScriptObject::getItems() const
{
    return mSession->getJson({ QModelIndex() });
}

void SessionScriptObject::updateItems(QJsonValue update)
{
    auto array = (update.isArray() ? update.toArray() : QJsonArray({ update }));
    mSession->dropJson(array, -1, { }, true);

    mPendingUpdates.push_back([this, array = std::move(array)]() {
        mSession->dropJson(array, -1, { }, true);
    });
}

void SessionScriptObject::deleteItem(QJsonValue item)
{
    mPendingUpdates.push_back([this, item = std::move(item)]() {
        const auto index = mSession->getIndex(findItem(item));
        if (index.isValid())
            mSession->deleteItem(index);
    });
}

void SessionScriptObject::setBufferData(QJsonValue item, QJSValue data)
{
    if (auto buffer = castItem<Buffer>(findItem(item)))
        if (!buffer->items.isEmpty())
            if (auto block = castItem<Block>(buffer->items[0]))
                return mPendingEditorUpdates.push_back([this, bufferId = buffer->id,
                        fileName = ensureFileName(*mSession, castItem<Buffer>(block->parent)), 
                        data = toByteArray(data, *block)]() {
                    if (auto buffer = castItem<Buffer>(mSession->findItem(bufferId))) {
                        mSession->setData(mSession->getIndex(buffer, SessionModel::FileName), fileName);
                        if (auto editor = openBinaryEditor(*buffer))
                            editor->replace(data, false);
                    }
                });

    mMessages += MessageList::insert(0,
        MessageType::ScriptError, "setBufferData failed");
}

void SessionScriptObject::setBlockData(QJsonValue item, QJSValue data)
{
    if (auto block = castItem<Block>(findItem(item)))
        return mPendingEditorUpdates.push_back([this, blockId = block->id,
                fileName = ensureFileName(*mSession, castItem<Buffer>(block->parent)), 
                data = toByteArray(data, *block)]() {
            if (auto block = castItem<Block>(mSession->findItem(blockId)))
                if (auto buffer = castItem<Buffer>(block->parent)) {
                    mSession->setData(mSession->getIndex(buffer, SessionModel::FileName), fileName);
                    if (auto editor = openBinaryEditor(*buffer)) {
                        const auto offset = Singletons::defaultScriptEngine().evaluateInt(
                            block->offset, 0, mMessages);
                        editor->replaceRange(offset, data, false);
                    }
                }
        });

    mMessages += MessageList::insert(0,
        MessageType::ScriptError, "setBlockData failed");
}

void SessionScriptObject::setTextureData(QJsonValue item, QJSValue data)
{
    if (auto texture = castItem<Texture>(findItem(item)))
        return mPendingEditorUpdates.push_back([this, textureId = texture->id,
                fileName = ensureFileName(*mSession, castItem<Buffer>(texture)),
                data = toTextureData(data, *texture)]() {
            if (auto texture = castItem<Texture>(mSession->findItem(textureId))) {
                mSession->setData(mSession->getIndex(texture, SessionModel::FileName), fileName);
                if (auto editor = openTextureEditor(*texture))
                    editor->replace(data, false);
            }
        });

    mMessages += MessageList::insert(0,
        MessageType::ScriptError, "setTextureData failed");
}

const Item *SessionScriptObject::findItem(QJsonValue objectOrId) const
{
    const auto id = (objectOrId.isObject() ?
        objectOrId.toObject()["id"].toInt() :
        objectOrId.toInt());
    return mSession->findItem(id);
}
