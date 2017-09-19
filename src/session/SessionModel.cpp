#include "SessionModel.h"
#include "FileDialog.h"
#include <QIcon>
#include <QMimeData>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>
#include <QDir>
#include <QMetaEnum>

namespace {
    template<typename T>
    auto toJsonValue(const T &v)
         -> std::enable_if_t<!std::is_enum<T>::value, QJsonValue>
    {
        return v;
    }

    template<typename T>
    auto toJsonValue(const T &v)
         -> std::enable_if_t<std::is_enum<T>::value, QJsonValue>
    {
        return QMetaEnum::fromType<T>().valueToKey(v);
    }

    template<>
    QJsonValue toJsonValue(const unsigned int &v)
    {
        return static_cast<double>(v);
    }

    template<typename T>
    auto fromJsonValue(const QJsonValue &value, T &v)
         -> std::enable_if_t<!std::is_enum<T>::value, bool>
    {
        if (!value.isUndefined()) {
            v = static_cast<T>(value.toDouble());
            return true;
        }
        return false;
    }

    template<typename T>
    auto fromJsonValue(const QJsonValue &key, T &v)
         -> std::enable_if_t<std::is_enum<T>::value, bool>
    {
        if (!key.isUndefined()) {
            auto ok = false;
            auto value = QMetaEnum::fromType<T>().keyToValue(
                qPrintable(key.toString()), &ok);
            if (ok) {
                v = static_cast<T>(value);
                return true;
            }
        }
        return false;
    }

    template<>
    bool fromJsonValue(const QJsonValue &value, QString &v)
    {
        if (!value.isUndefined()) {
            v = value.toString();
            return true;
        }
        return false;
    }

    template<>
    bool fromJsonValue(const QJsonValue &value, bool &v)
    {
        if (!value.isUndefined()) {
            v = value.toBool();
            return true;
        }
        return false;
    }


    template<>
    bool fromJsonValue(const QJsonValue &value, QColor &v)
    {
        if (!value.isUndefined()) {
            v = QColor(value.toString());
            return true;
        }
        return false;
    }
} // namespace

SessionModel::SessionModel(QObject *parent)
    : SessionModelCore(parent)
{
    mTypeIcons[Item::Type::Group].addFile(QStringLiteral(":/images/16x16/folder.png"));
    mTypeIcons[Item::Type::Buffer].addFile(QStringLiteral(":/images/16x16/x-office-spreadsheet.png"));
    mTypeIcons[Item::Type::Column].addFile(QStringLiteral(":/images/16x16/mail-attachment.png"));
    mTypeIcons[Item::Type::Texture].addFile(QStringLiteral(":/images/16x16/image-x-generic.png"));
    mTypeIcons[Item::Type::Image].addFile(QStringLiteral(":/images/16x16/mail-attachment.png"));
    mTypeIcons[Item::Type::Program].addFile(QStringLiteral(":/images/16x16/applications-system.png"));
    mTypeIcons[Item::Type::Shader].addFile(QStringLiteral(":/images/16x16/font.png"));
    mTypeIcons[Item::Type::Binding].addFile(QStringLiteral(":/images/16x16/insert-text.png"));
    mTypeIcons[Item::Type::VertexStream].addFile(QStringLiteral(":/images/16x16/media-playback-start-rtl.png"));
    mTypeIcons[Item::Type::Attribute].addFile(QStringLiteral(":/images/16x16/mail-attachment.png"));
    mTypeIcons[Item::Type::Target].addFile(QStringLiteral(":/images/16x16/emblem-photos.png"));
    mTypeIcons[Item::Type::Attachment].addFile(QStringLiteral(":/images/16x16/mail-attachment.png"));
    mTypeIcons[Item::Type::Call].addFile(QStringLiteral(":/images/16x16/dialog-information.png"));
    mTypeIcons[Item::Type::Script].addFile(QStringLiteral(":/images/16x16/font.png"));

    mActiveColor = QColor::fromRgb(0, 32, 255);
    mActiveCallFont = QFont();
    mActiveCallFont.setBold(true);
}

SessionModel::~SessionModel()
{
    clear();
}

QIcon SessionModel::getTypeIcon(Item::Type type) const
{
    return mTypeIcons[type];
}

QVariant SessionModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DecorationRole) {
        auto type = getItemType(index);
        return (type != Item::Type::Call ? getTypeIcon(type) : QVariant());
    }

    if (role != Qt::DisplayRole &&
        role != Qt::EditRole &&
        role != Qt::FontRole &&
        role != Qt::ForegroundRole &&
        role != Qt::CheckStateRole)
        return { };

    const auto &item = getItem(index);

    if (role == Qt::FontRole)
        return (item.type == Item::Type::Call &&
                mActiveItemIds.contains(item.id) ?
             mActiveCallFont : QVariant());

    if (role == Qt::ForegroundRole)
        return (mActiveItemIds.contains(item.id) ?
            mActiveColor : QVariant());

    return SessionModelCore::data(index, role);
}

Qt::ItemFlags SessionModel::flags(const QModelIndex &index) const
{
    Q_UNUSED(index);
    auto type = getItemType(index);
    auto flags = Qt::ItemFlags();
    flags |= Qt::ItemIsSelectable;
    flags |= Qt::ItemIsEnabled;
    flags |= Qt::ItemIsEditable;
    flags |= Qt::ItemIsDragEnabled;
    flags |= Qt::ItemIsUserCheckable;
    switch (type) {
        case Item::Type::Group:
        case Item::Type::Buffer:
        case Item::Type::Texture:
        case Item::Type::Program:
        case Item::Type::VertexStream:
        case Item::Type::Target:
            flags |= Qt::ItemIsDropEnabled;
            break;
        default:
            break;
    }
    // workaround to optimize D&D (do not snap to not droppable targets)
    if (!mDraggedIndices.empty() && (flags & Qt::ItemIsDropEnabled))
        if (!canContainType(index, getItemType(mDraggedIndices.first())))
            flags &= ~Qt::ItemIsDropEnabled;

    return flags;
}

bool SessionModel::removeRows(int row, int count, const QModelIndex &parent)
{
    SessionModelCore::removeRows(row, count, parent);
    mDraggedIndices.clear();
    return true;
}

void SessionModel::setActiveItems(QSet<ItemId> itemIds)
{
    if (mActiveItemIds == itemIds)
        return;

    mActiveItemIds = std::move(itemIds);
    emit dataChanged(index(0, 0), index(rowCount(), 0),
        { Qt::FontRole, Qt::ForegroundRole });
}

void SessionModel::setItemActive(ItemId id, bool active)
{
    if (active == mActiveItemIds.contains(id))
        return;

    if (active)
        mActiveItemIds.insert(id);
    else
        mActiveItemIds.remove(id);

    auto item = findItem(id);
    emit dataChanged(getIndex(item), getIndex(item),
        { Qt::FontRole, Qt::ForegroundRole });
}

 QString SessionModel::findItemName(ItemId id) const
 {
     if (auto item = findItem(id))
         return item->name;
     return { };
 }

QStringList SessionModel::mimeTypes() const
{
    return { QStringLiteral("text/plain") };
}

Qt::DropActions SessionModel::supportedDragActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::DropActions SessionModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

const QJsonDocument *SessionModel::parseClipboard(const QMimeData *data) const
{
    if (data != mClipboardData) {
        mClipboardData = data;
        mClipboardJsonDocument.reset();
        auto document = QJsonDocument::fromJson(data->text().toUtf8());
        if (!document.isNull())
            mClipboardJsonDocument.reset(new QJsonDocument(std::move(document)));
    }
    return mClipboardJsonDocument.data();
}

bool SessionModel::canDropMimeData(const QMimeData *data,
        Qt::DropAction action, int row, int column,
        const QModelIndex &parent) const
{
    if (!QAbstractItemModel::canDropMimeData(
            data, action, row, column, parent))
        return false;

    const auto getType = [&](const auto& object) {
        return getTypeByName(object["item"].toString());
    };

    if (auto document = parseClipboard(data)) {
        if (document->isArray()) {
            foreach (const QJsonValue &value, document->array())
                if (!canContainType(parent, getType(value.toObject())))
                    return false;
            return true;
        }
        else if (document->isObject()) {
            return canContainType(parent, getType(document->object()));
        }
    }
    return false;
}

void SessionModel::clear()
{
    undoStack().clear();
    undoStack().setUndoLimit(1);

    deleteItem(QModelIndex());

    undoStack().clear();
    undoStack().setUndoLimit(0);
}

bool SessionModel::save(const QString &fileName)
{
    QDir::setCurrent(QFileInfo(fileName).path());
    QScopedPointer<QMimeData> mime(mimeData({ QModelIndex() }));
    if (!mime)
        return false;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(mime->text().toUtf8());
    file.close();

    undoStack().setClean();
    return true;
}

bool SessionModel::load(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDir::setCurrent(QFileInfo(fileName).path());
    QMimeData data;
    data.setText(file.readAll());
    if (!canDropMimeData(&data, Qt::CopyAction, rowCount(), 0, {}))
        return false;

    undoStack().clear();
    undoStack().setUndoLimit(1);

    dropMimeData(&data, Qt::CopyAction, 0, 0, {});

    undoStack().clear();
    undoStack().setUndoLimit(0);
    return true;
}

QJsonArray SessionModel::getJson(const QModelIndexList &indexes) const
{
    auto itemArray = QJsonArray();
    if (indexes.size() == 1 && !indexes.first().isValid()) {
        foreach (const Item *item, getItem({ }).items) {
            auto object = QJsonObject();
            serialize(object, *item, true);
            itemArray.append(object);
        }
    }
    else {
        foreach (QModelIndex index, indexes) {
            auto object = QJsonObject();
            serialize(object, getItem(index), false);
            itemArray.append(object);
        }
    }
    return itemArray;
}

void SessionModel::dropJson(const QJsonDocument &document,
    int row, const QModelIndex &parent, bool updateExisting)
{
    if (document.isArray()) {
        foreach (const QJsonValue &value, document.array())
            deserialize(value.toObject(), parent, row++, updateExisting);
    }
    else if (document.isObject()) {
        deserialize(document.object(), parent, row, updateExisting);
    }

    // fixup item references
    foreach (ItemId prevId, mDroppedIdsReplaced.keys())
        foreach (ItemId* reference, mDroppedReferences)
            if (*reference == prevId)
                *reference = mDroppedIdsReplaced[prevId];
    mDroppedIdsReplaced.clear();
    mDroppedReferences.clear();
}

QMimeData *SessionModel::mimeData(const QModelIndexList &indexes) const
{
    auto data = new QMimeData();
    auto itemArray = getJson(indexes);
    auto document =
        (itemArray.size() != 1 ?
         QJsonDocument(itemArray) :
         QJsonDocument(itemArray.first().toObject()));
    data->setText(document.toJson());
    mDraggedIndices = indexes;
    return data;
}

bool SessionModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
        int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(column);
    if (action == Qt::IgnoreAction)
        return true;

    if (row < 0)
        row = rowCount(parent);

    undoStack().beginMacro("Move");

    if (action == Qt::MoveAction && !mDraggedIndices.empty()) {
        std::stable_sort(mDraggedIndices.begin(), mDraggedIndices.end(),
            [](const auto &a, const auto &b) { return a.row() > b.row(); });
        for (auto index : mDraggedIndices) {
            if (index.parent() == parent &&
                index.row() < row)
                --row;
            deleteItem(index);
        }
    }

    auto document = parseClipboard(data);
    if (!document)
        return false;

    dropJson(*document, row, parent, false);
    mDraggedIndices.clear();

    undoStack().endMacro();
    return true;
}

void SessionModel::serialize(QJsonObject &object, const Item &item,
    bool relativeFilePaths) const
{
    const auto write = [&](const char *name, const auto &property) {
        object.insert(name, toJsonValue(property));
    };
    const auto writeFileName = [&](const char *name, const auto &property) {
        if (!FileDialog::isEmptyOrUntitled(property))
            write(name, relativeFilePaths ?
                QDir::current().relativeFilePath(property) : property);
    };

    write("type", getTypeName(item.type));
    if (item.id)
        write("id", item.id);
    if (!item.name.isEmpty())
        write("name", item.name);

    switch (item.type) {
        case Item::Type::Group: {
            const auto &group = static_cast<const Group&>(item);
            write("inline", group.inlineScope);
            break;
        }

        case Item::Type::Buffer: {
            const auto &buffer = static_cast<const Buffer&>(item);
            writeFileName("fileName", buffer.fileName);
            write("offset", buffer.offset);
            write("rowCount", buffer.rowCount);
            break;
        }

        case Item::Type::Column: {
            const auto &column = static_cast<const Column&>(item);
            write("dataType", column.dataType);
            write("count", column.count);
            write("padding", column.padding);
            break;
        }

        case Item::Type::Texture: {
            const auto &texture = static_cast<const Texture&>(item);
            auto kind = getKind(texture);
            writeFileName("fileName", texture.fileName);
            write("target", texture.target);
            write("format",  texture.format);
            write("width",  texture.width);
            if (kind.dimensions > 1)
                write("height",  texture.height);
            if (kind.dimensions > 2)
                write("depth", texture.depth);
            if (kind.array)
                write("layers", texture.layers);
            if (kind.multisample)
                write("samples", texture.samples);
            if (kind.dimensions > 1 && texture.flipY)
                write("flipY", texture.flipY);
            break;
        }

        case Item::Type::Image: {
            const auto &image = static_cast<const Image&>(item);
            writeFileName("fileName", image.fileName);
            write("level", image.level);
            write("layer", image.layer);
            write("face", image.face);
            break;
        }

        case Item::Type::Program: {
            break;
        }

        case Item::Type::Shader: {
            const auto &shader = static_cast<const Shader&>(item);
            write("shaderType", shader.shaderType);
            writeFileName("fileName", shader.fileName);
            break;
        }

        case Item::Type::Binding: {
            const auto &binding = static_cast<const Binding&>(item);
            write("bindingType", binding.bindingType);
            if (binding.bindingType == Binding::BindingType::Uniform)
                write("editor", binding.editor);
            auto values = QJsonArray();
            for (auto i = 0; i < binding.valueCount; ++i) {
                const auto &value = binding.values[i];
                auto v = QJsonObject();
                switch (binding.bindingType) {
                    case Binding::BindingType::Uniform: {
                        auto fields = QJsonArray();
                        foreach (const QString &field, value.fields)
                            fields.append(field);
                        v.insert("fields", fields);
                        break;
                    }
                    case Binding::BindingType::Image:
                        v.insert("textureId", value.textureId);
                        v.insert("level", value.level);
                        v.insert("layered", value.layered);
                        v.insert("layer", value.layer);
                        break;

                    case Binding::BindingType::Sampler:
                        v.insert("textureId", value.textureId);
                        v.insert("minFilter", toJsonValue(value.minFilter));
                        v.insert("magFilter", toJsonValue(value.magFilter));
                        v.insert("wrapModeX", toJsonValue(value.wrapModeX));
                        v.insert("wrapModeY", toJsonValue(value.wrapModeY));
                        v.insert("wrapModeZ", toJsonValue(value.wrapModeZ));
                        v.insert("wrapModeZ", toJsonValue(value.wrapModeZ));
                        v.insert("borderColor", toJsonValue(
                            value.borderColor.name(QColor::HexArgb)));
                        break;

                    case Binding::BindingType::Buffer:
                        v.insert("bufferId", value.bufferId);
                        break;

                    case Binding::BindingType::Subroutine:
                        v.insert("subroutine", value.subroutine);
                        break;
                }
                values.append(v);
            }
            object.insert("values", values);
            break;
        }

        case Item::Type::VertexStream: {
            break;
        }

        case Item::Type::Attribute: {
            const auto &attribute = static_cast<const Attribute&>(item);
            write("bufferId", attribute.bufferId);
            write("columnId", attribute.columnId);
            write("normalize", attribute.normalize);
            write("divisor", attribute.divisor);
            break;
        }

        case Item::Type::Target: {
            const auto &target = static_cast<const Target&>(item);
            write("frontFace", target.frontFace);
            write("cullMode", target.cullMode);
            write("logicOperation", target.logicOperation);
            write("blendConstant", target.blendConstant.name(QColor::HexArgb));
            break;
        }

        case Item::Type::Attachment: {
            const auto &attachment = static_cast<const Attachment&>(item);
            auto kind = TextureKind();
            if (auto texture = findItem<Texture>(attachment.textureId))
                kind = getKind(*texture);

            write("textureId", attachment.textureId);
            write("level", attachment.level);
            if (kind.array) {
              write("layered", attachment.layered);
              write("layer", attachment.layer);
            }
            if (kind.color) {
                write("blendColorEq", attachment.blendColorEq);
                write("blendColorSource", attachment.blendColorSource);
                write("blendColorDest", attachment.blendColorDest);
                write("blendAlphaEq", attachment.blendAlphaEq);
                write("blendAlphaSource", attachment.blendAlphaSource);
                write("blendAlphaDest", attachment.blendAlphaDest);
                write("colorWriteMask", attachment.colorWriteMask);
            }
            if (kind.depth) {
                write("depthComparisonFunc", attachment.depthComparisonFunc);
                write("depthOffsetFactor", attachment.depthOffsetFactor);
                write("depthOffsetUnits", attachment.depthOffsetUnits);
                write("depthClamp", attachment.depthClamp);
                write("depthWrite", attachment.depthWrite);
            }
            if (kind.stencil) {
                write("stencilFrontComparisonFunc", attachment.stencilFrontComparisonFunc);
                write("stencilFrontReference", attachment.stencilFrontReference);
                write("stencilFrontReadMask", attachment.stencilFrontReadMask);
                write("stencilFrontFailOp", attachment.stencilFrontFailOp);
                write("stencilFrontDepthFailOp", attachment.stencilFrontDepthFailOp);
                write("stencilFrontDepthPassOp", attachment.stencilFrontDepthPassOp);
                write("stencilFrontWriteMask", attachment.stencilFrontWriteMask);
                write("stencilBackComparisonFunc", attachment.stencilBackComparisonFunc);
                write("stencilBackReference", attachment.stencilBackReference);
                write("stencilBackReadMask", attachment.stencilBackReadMask);
                write("stencilBackFailOp", attachment.stencilBackFailOp);
                write("stencilBackDepthFailOp", attachment.stencilBackDepthFailOp);
                write("stencilBackDepthPassOp", attachment.stencilBackDepthPassOp);
                write("stencilBackWriteMask", attachment.stencilBackWriteMask);
            }
            break;
        }

        case Item::Type::Call: {
            const auto &call = static_cast<const Call&>(item);
            const auto kind = getKind(call);
            const auto type = call.callType;

            write("checked", call.checked);
            write("callType", call.callType);
            if (kind.draw || kind.compute)
                write("programId", call.programId);
            if (kind.draw) {
                write("targetId", call.targetId);
                write("vertexStreamId", call.vertexStreamId);
                write("primitiveType", call.primitiveType);
            }
            if (kind.drawPatches)
                write("patchVertices", call.patchVertices);
            if (kind.drawIndexed)
                write("indexBufferId", call.indexBufferId);
            if (kind.drawDirect) {
                write("count", call.count);
                write("first", call.first);
                write("instanceCount", call.instanceCount);
                write("baseInstance", call.baseInstance);
                if (kind.drawIndexed)
                    write("baseVertex", call.baseVertex);
            }
            if (kind.drawIndirect) {
                write("indirectBufferId", call.indirectBufferId);
                write("drawCount", call.drawCount);
            }
            if (kind.compute) {
                write("workGroupsX", call.workGroupsX);
                write("workGroupsY", call.workGroupsY);
                write("workGroupsZ", call.workGroupsZ);
            }
            if (type == Call::CallType::ClearTexture ||
                type == Call::CallType::GenerateMipmaps)
                write("textureId", call.textureId);
            if (type == Call::CallType::ClearTexture) {
                write("clearColor", call.clearColor.name(QColor::HexArgb));
                write("clearDepth", call.clearDepth);
                write("clearStencil", call.clearStencil);
            }
            if (type == Call::CallType::ClearBuffer)
                write("bufferId", call.bufferId);
            break;
        }

        case Item::Type::Script: {
            const auto &script = static_cast<const Script&>(item);
            writeFileName("fileName", script.fileName);
            break;
        }
    }

    if (!item.items.empty()) {
        auto items = QJsonArray();
        foreach (const Item *item, item.items) {
            auto sub = QJsonObject();
            serialize(sub, *item, relativeFilePaths);
            items.append(sub);
        }
        object["items"] = items;
    }
}

void SessionModel::deserialize(const QJsonObject &object,
    const QModelIndex &parent, int row, bool updateExisting)
{
    const auto readValue = [&](const QJsonValue& value, auto &property) {
        return fromJsonValue(value, property);
    };
    const auto readRefValue = [&](const QJsonValue& value, ItemId &id) {
        if (fromJsonValue(value, id))
            mDroppedReferences.append(&id);
    };
    const auto read = [&](auto name, auto &property) {
        readValue(object[name], property);
    };
    const auto readFileName = [&](auto name, auto &property) {
        if (readValue(object[name], property))
            property = QDir::current().absoluteFilePath(property);
    };
    const auto readRef = [&](auto name, ItemId &id) {
        readRefValue(object[name], id);
    };

    auto type = getTypeByName(object["type"].toString());
    if (!canContainType(parent, type))
        return;

    auto id = object["id"].toInt();
    if (!id)
        id = getNextItemId();

    auto existingItem = findItem(id);
    if (existingItem && !updateExisting) {
        // generate new id when it collides with an existing item
        auto prevId = std::exchange(id, getNextItemId());
        mDroppedIdsReplaced.insert(prevId, id);
        existingItem = nullptr;
    }
    const auto index = (existingItem ?
        getIndex(existingItem) : insertItem(type, parent, row, id));

    auto &item = getItem(index);
    read("name", item.name);

    switch (item.type) {
        case Item::Type::Group: {
            auto &group = static_cast<Group&>(item);
            read("inline", group.inlineScope);
            break;
        }

        case Item::Type::Buffer: {
            auto &buffer = static_cast<Buffer&>(item);
            readFileName("fileName", buffer.fileName);
            read("offset", buffer.offset);
            read("rowCount", buffer.rowCount);
            break;
        }

        case Item::Type::Column: {
            auto &column = static_cast<Column&>(item);
            read("dataType", column.dataType);
            read("count", column.count);
            read("padding", column.padding);
            break;
        }

        case Item::Type::Texture: {
            auto &texture = static_cast<Texture&>(item);
            readFileName("fileName", texture.fileName);
            read("target", texture.target);
            read("format", texture.format);
            read("width", texture.width);
            read("height", texture.height);
            read("depth", texture.depth);
            read("layers", texture.layers);
            read("samples", texture.samples);
            read("flipY", texture.flipY);
            break;
        }

        case Item::Type::Image: {
            auto &image = static_cast<Image&>(item);
            readFileName("fileName", image.fileName);
            read("level", image.level);
            read("layer", image.layer);
            read("face", image.face);
            break;
        }

        case Item::Type::Program: {
            break;
        }

        case Item::Type::Shader: {
            auto &shader = static_cast<Shader&>(item);
            read("shaderType", shader.shaderType);
            readFileName("fileName", shader.fileName);
            break;
        }

        case Item::Type::Binding: {
            auto &binding = static_cast<Binding&>(item);
            read("bindingType", binding.bindingType);
            read("editor", binding.editor);
            auto values = object["values"].toArray();
            binding.valueCount =
                qMin(values.size(), static_cast<int>(binding.values.size()));
            for (auto i = 0; i < binding.valueCount; ++i) {
                const auto v = values[i].toObject();
                auto &value = binding.values[i];
                foreach (const QJsonValue &field, v["fields"].toArray())
                    value.fields.append(field.toString());
                readRefValue(v["textureId"], value.textureId);
                readRefValue(v["bufferId"], value.bufferId);
                readValue(v["level"], value.level);
                readValue(v["layered"], value.layered);
                readValue(v["layer"], value.layer);
                readValue(v["minFilter"], value.minFilter);
                readValue(v["magFilter"], value.magFilter);
                readValue(v["wrapModeX"], value.wrapModeX);
                readValue(v["wrapModeY"], value.wrapModeY);
                readValue(v["wrapModeZ"], value.wrapModeZ);
                readValue(v["borderColor"], value.borderColor);
                readValue(v["subroutine"], value.subroutine);
            }
            break;
        }

        case Item::Type::VertexStream: {
            break;
        }

        case Item::Type::Attribute: {
            auto &attribute = static_cast<Attribute&>(item);
            readRef("bufferId", attribute.bufferId);
            readRef("columnId", attribute.columnId);
            read("normalize", attribute.normalize);
            read("divisor", attribute.divisor);
            break;
        }

        case Item::Type::Target: {
            auto &target = static_cast<Target&>(item);
            read("frontFace", target.frontFace);
            read("cullMode", target.cullMode);
            read("logicOperation", target.logicOperation);
            read("blendConstant", target.blendConstant);
            break;
        }

        case Item::Type::Attachment: {
            auto &attachment = static_cast<Attachment&>(item);
            readRef("textureId", attachment.textureId);
            read("level", attachment.level);
            read("layered", attachment.layered);
            read("layer", attachment.layer);
            read("blendColorEq", attachment.blendColorEq);
            read("blendColorSource", attachment.blendColorSource);
            read("blendColorDest", attachment.blendColorDest);
            read("blendAlphaEq", attachment.blendAlphaEq);
            read("blendAlphaSource", attachment.blendAlphaSource);
            read("blendAlphaDest", attachment.blendAlphaDest);
            read("colorWriteMask", attachment.colorWriteMask);
            read("depthComparisonFunc", attachment.depthComparisonFunc);
            read("depthOffsetFactor", attachment.depthOffsetFactor);
            read("depthOffsetUnits", attachment.depthOffsetUnits);
            read("depthClamp", attachment.depthClamp);
            read("depthWrite", attachment.depthWrite);
            read("stencilFrontComparisonFunc", attachment.stencilFrontComparisonFunc);
            read("stencilFrontReference", attachment.stencilFrontReference);
            read("stencilFrontReadMask", attachment.stencilFrontReadMask);
            read("stencilFrontFailOp", attachment.stencilFrontFailOp);
            read("stencilFrontDepthFailOp", attachment.stencilFrontDepthFailOp);
            read("stencilFrontDepthPassOp", attachment.stencilFrontDepthPassOp);
            read("stencilFrontWriteMask", attachment.stencilFrontWriteMask);
            read("stencilBackComparisonFunc", attachment.stencilBackComparisonFunc);
            read("stencilBackReference", attachment.stencilBackReference);
            read("stencilBackReadMask", attachment.stencilBackReadMask);
            read("stencilBackFailOp", attachment.stencilBackFailOp);
            read("stencilBackDepthFailOp", attachment.stencilBackDepthFailOp);
            read("stencilBackDepthPassOp", attachment.stencilBackDepthPassOp);
            read("stencilBackWriteMask", attachment.stencilBackWriteMask);
            break;
        }

        case Item::Type::Call: {
            auto &call = static_cast<Call&>(item);
            read("checked", call.checked);
            read("callType", call.callType);
            readRef("programId", call.programId);
            readRef("vertexStreamId", call.vertexStreamId);
            readRef("targetId", call.targetId);
            readRef("indexBufferId", call.indexBufferId);
            read("primitiveType", call.primitiveType);
            read("patchVertices", call.patchVertices);
            read("count", call.count);
            read("first", call.first);
            read("baseVertex", call.baseVertex);
            read("instanceCount", call.instanceCount);
            read("baseInstance", call.baseInstance);
            readRef("indirectBufferId", call.indirectBufferId);
            read("drawCount", call.drawCount);
            read("workGroupsX", call.workGroupsX);
            read("workGroupsY", call.workGroupsY);
            read("workGroupsZ", call.workGroupsZ);
            readRef("textureId", call.textureId);
            read("clearColor", call.clearColor);
            read("clearDepth", call.clearDepth);
            read("clearStencil", call.clearStencil);
            readRef("bufferId", call.bufferId);
            break;
        }

        case Item::Type::Script: {
            auto &texture = static_cast<Texture&>(item);
            readFileName("fileName", texture.fileName);
            break;
        }
    }

    auto items = object["items"].toArray();
    if (!items.isEmpty())
        foreach (const QJsonValue &value, items)
            deserialize(value.toObject(), index, -1, updateExisting);
}
