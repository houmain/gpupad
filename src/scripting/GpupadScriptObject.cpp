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
#include <QTextStream>
#include <QFloat16>
#include <cstring>

#if defined(QtWebEngineWidgets_FOUND)
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

    const Item *findItem(QJsonValue objectOrId)
    {
        const auto id = (objectOrId.isObject() ?
            objectOrId.toObject()["id"].toInt() :
            objectOrId.toInt());
        return Singletons::sessionModel().findItem(id);
    }

    void ensureFileName(const FileItem &item)
    {
        if (item.fileName.isEmpty()) {
            const auto fileName = FileDialog::generateNextUntitledFileName(item.name);
            Singletons::sessionModel().setData(Singletons::sessionModel().getIndex(
                &item, SessionModel::FileName), fileName);
        }
    }

    BinaryEditor *openBinaryEditor(const Buffer &buffer)
    {
        ensureFileName(buffer);
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
        ensureFileName(texture);
        auto &editors = Singletons::editorManager();
        editors.setAutoRaise(false);
        auto editor = editors.openTextureEditor(texture.fileName);
        if (!editor)
            editor = editors.openNewTextureEditor(texture.fileName);
        editors.setAutoRaise(true);
        return editor;
    }
} // namespace

GpupadScriptObject::GpupadScriptObject(ScriptEngine *scriptEngine) 
    : QObject(scriptEngine)
    , mScriptEngine(*scriptEngine)
{
    auto file = QFile(":/scripting/GpupadScriptObject.js");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        mScriptEngine.setGlobal("gpupad", this);
        mScriptEngine.evaluateScript(QTextStream(&file).readAll(), "", mMessages);
    }
}

void GpupadScriptObject::applySessionUpdate(ScriptEngine &scriptEngine)
{
    scriptEngine.evaluateScript("gpupad.updateItems()", "", mMessages);

    if (!mPendingUpdates.empty()) {
        Singletons::sessionModel().beginUndoMacro("Script");
        for (const auto &update : mPendingUpdates)
            update();
        mPendingUpdates.clear();
        Singletons::sessionModel().endUndoMacro();
    }

    for (const auto &update : mPendingEditorUpdates)
        update();
    mPendingEditorUpdates.clear();

    scriptEngine.updateVariables(mMessages);

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

void GpupadScriptObject::setBufferData(QJsonValue item, QJSValue data)
{
    if (!castItem<Buffer>(findItem(item))) {
        mMessages += MessageList::insert(0, MessageType::ScriptError,
            "setBufferData failed. Invalid item");
        return;
    }

    mPendingEditorUpdates.push_back([this, item = std::move(item), data = std::move(data)]() {
        if (auto buffer = castItem<Buffer>(findItem(item)))
            if (auto editor = openBinaryEditor(*buffer))
                if (buffer->items.size() >= 1)
                    if (auto block = castItem<Block>(buffer->items[0])) {
                        editor->replaceRange(0, toByteArray(data, *block), false);
                        mEditorDataUpdated = true;
                        return;
                    }

        mMessages += MessageList::insert(0, MessageType::ScriptError,
            "setBufferData failed");
    });
}

void GpupadScriptObject::setBlockData(QJsonValue item, QJSValue data)
{
    if (!castItem<Block>(findItem(item))) {
        mMessages += MessageList::insert(0, MessageType::ScriptError,
            "setBlockData failed. Invalid item");
        return;
    }

    mPendingEditorUpdates.push_back([this, item = std::move(item), data = std::move(data)]() {
        if (auto block = castItem<Block>(findItem(item)))
            if (auto editor = openBinaryEditor(*castItem<Buffer>(block->parent))) {
                auto ok = true;
                const auto offset = (block->evaluatedOffset >= 0 ? block->evaluatedOffset : 
                    evaluateIntExpression(block->offset, &ok));
                if (ok) {
                    editor->replaceRange(offset, toByteArray(data, *block), false);
                    mEditorDataUpdated = true;
                    return;
                }
            }

        mMessages += MessageList::insert(0, MessageType::ScriptError,
            "setBlockData failed");
    });
}

void GpupadScriptObject::setTextureData(QJsonValue item, QJSValue data)
{
    if (!castItem<Block>(findItem(item))) {
        mMessages += MessageList::insert(0, MessageType::ScriptError,
            "setTextureData failed. Invalid item");
        return;
    }

    mPendingEditorUpdates.push_back([this, item = std::move(item), data = std::move(data)]() {
        if (auto texture = castItem<Texture>(findItem(item)))
            if (auto editor = openTextureEditor(*texture)) {
                editor->replace(toTextureData(data, *texture), false);
                mEditorDataUpdated = true;
                return;
            }
        mMessages += MessageList::insert(0, MessageType::ScriptError,
            "setTextureData failed");
    });
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
#if defined(QtWebEngineWidgets_FOUND)
    class CustomEditor : public IEditor
    {
    public:
        QList<QMetaObject::Connection> connectEditActions(const EditActions &) override { return { }; }
        QString fileName() const override { return FileDialog::generateNextUntitledFileName("Web"); }
        void setFileName(QString) override { }
        bool load() override { return false; }
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
