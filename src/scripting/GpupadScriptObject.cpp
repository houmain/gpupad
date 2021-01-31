#include "GpupadScriptObject.h"
#include "Singletons.h"
#include "ScriptEngine.h"
#include "FileDialog.h"
#include "FileCache.h"
#include "session/SessionModel.h"
#include "editors/EditorManager.h"
#include "editors/BinaryEditor.h"
#include <QJsonDocument>
#include <QVariantMap>
#include <QDir>
#include <cstring>

#if defined(Qt5WebEngineWidgets_FOUND)
#  include <QWebEngineView>
#  include <QWebChannel>
#endif

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

    const Item *findItem(QJsonValue objectOrId)
    {
        const auto id = (objectOrId.isObject() ?
            objectOrId.toObject()["id"].toInt() :
            objectOrId.toInt());
        return Singletons::sessionModel().findItem(id);
    }
} // namespace

GpupadScriptObject::GpupadScriptObject(QObject *parent) : QObject(parent)
{
}

void GpupadScriptObject::initialize(ScriptEngine &scriptEngine)
{
    auto file = QFile(":/scripting/GpupadScriptObject.js");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        scriptEngine.setGlobal("gpupad", this);
        scriptEngine.evaluateScript(QTextStream(&file).readAll(), "", mMessages);
    }
}

void GpupadScriptObject::applySessionUpdate(ScriptEngine &scriptEngine)
{
    scriptEngine.evaluateScript("gpupad.updateItems()", "", mMessages);

    if (mPendingUpdates.empty())
        return;

    Singletons::sessionModel().beginUndoMacro("Script");
    for (const auto &update : mPendingUpdates)
        update();
    mPendingUpdates.clear();
    Singletons::sessionModel().endUndoMacro();

    if (std::exchange(mEditorDataUpdated, false))
        Singletons::fileCache().updateEditorFiles();
}

QJsonArray GpupadScriptObject::getItems() const
{
    return Singletons::sessionModel().getJson({ QModelIndex() });
}

void GpupadScriptObject::updateItems(QJsonValue update)
{
    mPendingUpdates.push_back([update = std::move(update)]() {
        auto array = (update.isArray() ? update.toArray() : QJsonArray({ update }));
        Singletons::sessionModel().dropJson(array, -1, { }, true);
    });
}

void GpupadScriptObject::deleteItem(QJsonValue item)
{
    mPendingUpdates.push_back([item = std::move(item)]() {
        const auto index = Singletons::sessionModel().getIndex(findItem(item));
        if (index.isValid())
            Singletons::sessionModel().deleteItem(index);
    });
}

void GpupadScriptObject::setBlockData(QJsonValue item, QJSValue data)
{
    if (auto block = castItem<Block>(findItem(item))) {
        auto buffer = castItem<Buffer>(block->parent);
        if (buffer->fileName.isEmpty()) {
            const auto fileName = FileDialog::generateNextUntitledFileName(buffer->name);
            Singletons::sessionModel().setData(Singletons::sessionModel().getIndex(
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
            mEditorDataUpdated = true;
        }
    }
}

QString GpupadScriptObject::openFileDialog()
{
    auto options = FileDialog::Options();
    if (Singletons::fileDialog().exec(options))
        return Singletons::fileDialog().fileName();
    return { };
}

QString GpupadScriptObject::readTextFile(const QString &fileName)
{
    auto source = QString();
    if (Singletons::fileCache().getSource(fileName, &source))
        return source;
    return { };
}

bool GpupadScriptObject::openWebDock()
{
#if defined(Qt5WebEngineWidgets_FOUND)
    class CustomEditor : public IEditor
    {
    public:
        QList<QMetaObject::Connection> connectEditActions(const EditActions &) override { return { }; }
        QString fileName() const override { return { }; }
        void setFileName(QString) override { }
        bool load() override { return false; }
        bool reload() override { return false; }
        bool save() override { return false; }
        int tabifyGroup() override { return 1; }
    };

    auto fileName = QDir::current().filePath("index.html");
    if (!QFileInfo::exists(fileName))
        return false;

    auto editor = new CustomEditor();
    auto view = new QWebEngineView();
    Singletons::editorManager().createDock(view, editor);

    view->load(QUrl::fromLocalFile(fileName));
    connect(view, &QWebEngineView::loadStarted,
        [this, view]() {
            auto webChannel = new QWebChannel();
            webChannel->registerObject("gpupad", this);
            view->page()->setWebChannel(webChannel);
        });

    return true;
#else
    return false;
#endif
}
