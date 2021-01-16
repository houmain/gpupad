#include "GpupadScriptObject.h"
#include "Singletons.h"
#include "FileDialog.h"
#include "FileCache.h"
#include "session/SessionModel.h"
#include "editors/EditorManager.h"
#include "editors/BinaryEditor.h"
#include <QJsonDocument>
#include <QVariantMap>
#include <cstring>

namespace {
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
} // namespace

GpupadScriptObject::GpupadScriptObject(QObject *parent) : QObject(parent)
{
}

QJsonArray GpupadScriptObject::getItems() const
{
    return sessionModel().getJson({ QModelIndex() });
}

QJsonArray GpupadScriptObject::getItemsByName(const QString &name) const
{
    auto itemIndices = QList<QModelIndex>();
    sessionModel().forEachItem([&](const Item &item) {
        if (item.name == name)
            itemIndices.append(sessionModel().getIndex(&item));
    });
    return sessionModel().getJson(itemIndices);
}

void GpupadScriptObject::updateItems(QJsonValue update, QJsonValue parent, int row)
{
    const auto index = sessionModel().getIndex(findItem(parent));
    if (!update.isArray())
        update = QJsonArray({ update });
    sessionModel().dropJson(update.toArray(), row, index, true);
}

void GpupadScriptObject::deleteItem(QJsonValue item)
{
    sessionModel().deleteItem(sessionModel().getIndex(findItem(item)));
}

void GpupadScriptObject::setBlockData(QJsonValue item, QJSValue data)
{
    if (auto block = castItem<Block>(findItem(item))) {
        auto buffer = castItem<Buffer>(block->parent);
        if (buffer->fileName.isEmpty()) {
            const auto fileName = FileDialog::generateNextUntitledFileName(buffer->name);
            Singletons::sessionModel().setData(sessionModel().getIndex(
                buffer, SessionModel::FileName), fileName);
        }

        auto &editors = Singletons::editorManager();
        editors.setAutoRaise(false);
        auto editor = editors.openBinaryEditor(buffer->fileName);
        if (!editor)
            editor = editors.openNewBinaryEditor(buffer->fileName);
        editors.setAutoRaise(true);

        if (editor) {
            // TODO: take offset into account
            editor->replace(toByteArray(data, *block), false);
        }
    }
}

QJsonValue GpupadScriptObject::openFileDialog()
{
    auto options = FileDialog::Options();
    if (Singletons::fileDialog().exec(options))
        return Singletons::fileDialog().fileName();
    return { };
}

QJsonValue GpupadScriptObject::readTextFile(const QString &fileName)
{
    auto source = QString();
    if (Singletons::fileCache().getSource(fileName, &source))
        return source;
    return { };
}

SessionModel &GpupadScriptObject::sessionModel()
{
    if (!std::exchange(mUpdatingSession, true))
        Singletons::sessionModel().beginUndoMacro("Script");

    return Singletons::sessionModel();
}

const SessionModel &GpupadScriptObject::sessionModel() const
{
    return Singletons::sessionModel();
}

const Item *GpupadScriptObject::findItem(QJsonValue objectOrId) const
{
    const auto id = (objectOrId.isObject() ?
        objectOrId.toObject()["id"].toInt() :
        objectOrId.toInt());
    return sessionModel().findItem(id);
}

void GpupadScriptObject::finishSessionUpdate()
{
    if (std::exchange(mUpdatingSession, false)) {
        Singletons::sessionModel().endUndoMacro();

        // TODO: call when buffers were updated
        Singletons::fileCache().updateEditorFiles();
    }
}
