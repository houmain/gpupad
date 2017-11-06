#include "SessionModel.h"
#include "SessionModelPriv.h"
#include "FileDialog.h"
#include <QIcon>
#include <QMimeData>
#include <QJsonDocument>
#include <QDir>

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
    mTypeIcons[Item::Type::Stream].addFile(QStringLiteral(":/images/16x16/media-playback-start-rtl.png"));
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
        case Item::Type::Stream:
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

QJsonArray SessionModel::parseClipboard(const QMimeData *data) const
{
    if (mClipboardData != data) {
        mClipboardData = data;
        auto text = data->text().toUtf8();
        if (text != mClipboardText) {
            mClipboardText = text;
            auto document = QJsonDocument::fromJson(text);
            mClipboardJson =
                document.isNull() ? QJsonArray() :
                document.isArray() ? document.array() :
                QJsonArray({ document.object() });
        }
    }
    return mClipboardJson;
}

bool SessionModel::canDropMimeData(const QMimeData *data,
        Qt::DropAction action, int row, int column,
        const QModelIndex &parent) const
{
    if (!QAbstractItemModel::canDropMimeData(
            data, action, row, column, parent))
        return false;

    auto jsonArray = parseClipboard(data);
    if (jsonArray.empty())
        return false;

    foreach (const QJsonValue &value, jsonArray) {
        auto type = getTypeByName(value.toObject()["type"].toString());
        if (!canContainType(parent, type))
            return false;
    }
    return true;
}

void SessionModel::clear()
{
    undoStack().clear();
    undoStack().setUndoLimit(1);

    deleteItem(QModelIndex());

    undoStack().clear();
    undoStack().setUndoLimit(0);
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

QMimeData *SessionModel::mimeData(const QModelIndexList &indexes) const
{
    if (mDraggedIndices != indexes) {
      auto jsonArray = getJson(indexes);
      auto document =
          (jsonArray.size() != 1 ?
           QJsonDocument(jsonArray) :
           QJsonDocument(jsonArray.first().toObject()));
      mDraggedJson = document.toJson();
      mDraggedIndices = indexes;
    }
    auto data = new QMimeData();
    data->setText(mDraggedJson);
    return data;
}

bool SessionModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
        int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(column);
    if (action == Qt::IgnoreAction)
        return true;

    auto jsonArray = parseClipboard(data);
    if (jsonArray.empty())
        return false;

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

    dropJson(jsonArray, row, parent, false);
    mDraggedIndices.clear();

    undoStack().endMacro();
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

void SessionModel::dropJson(const QJsonArray &jsonArray,
    int row, const QModelIndex &parent, bool updateExisting)
{
    foreach (const QJsonValue &value, jsonArray)
        deserialize(value.toObject(), parent, row++, updateExisting);

    // fixup item references
    foreach (ItemId prevId, mDroppedIdsReplaced.keys())
        foreach (QModelIndex reference, mDroppedReferences)
            if (data(reference).toInt() == prevId)
                setData(reference, mDroppedIdsReplaced[prevId]);
    mDroppedIdsReplaced.clear();
    mDroppedReferences.clear();
}

void SessionModel::deserialize(const QJsonObject &object,
    const QModelIndex &parent, int row, bool updateExisting)
{
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

    const auto dropColumn = [&](const QString &property,
            const QModelIndex &index, const QVariant &value) {
        if (property.endsWith("Id"))
            mDroppedReferences.append(index);
        setData(index, value);
    };

    foreach (const QString &property, object.keys()) {
        auto value = object[property].toVariant();

        if (property == "name") {
            setData(getIndex(index, Name), value);
        }
        else if (property == "fileName") {
            setData(getIndex(index, FileName),
                QDir::current().absoluteFilePath(value.toString()));
        }
#define ADD(COLUMN_TYPE, ITEM_TYPE, PROPERTY) \
        else if (Item::Type::ITEM_TYPE == type && #PROPERTY == property) \
            dropColumn(property, getIndex(index, COLUMN_TYPE), value);

        ADD_EACH_COLUMN_TYPE()
#undef ADD
        else if (property == "values") {
            auto values = object[property].toArray();
            for (auto i = 0; i < values.size(); i++) {
                auto value = values[i].toObject();
                setData(getIndex(index, BindingCurrentValue), i);
                foreach (const QString &valueProperty, value.keys()) {
#define ADD(COLUMN_TYPE, PROPERTY) \
                    if (#PROPERTY == valueProperty) \
                        dropColumn(valueProperty, getIndex(index, COLUMN_TYPE), \
                            value[valueProperty].toVariant());

                    ADD_EACH_BINDING_VALUE_COLUMN_TYPE()
#undef ADD
                }
            }
            setData(getIndex(index, BindingCurrentValue), 0);
        }
    }

    auto items = object["items"].toArray();
    if (!items.isEmpty())
        foreach (const QJsonValue &value, items)
            deserialize(value.toObject(), index, -1, updateExisting);
}

void SessionModel::serialize(QJsonObject &object, const Item &item,
    bool relativeFilePaths) const
{
    object["type"] = getTypeName(item.type);
    object["id"] = item.id;
    object["name"] = item.name;
    if (auto fileItem = castItem<FileItem>(item)) {
        const auto &fileName = fileItem->fileName;
        if (!FileDialog::isEmptyOrUntitled(fileName))
            object["fileName"] = (relativeFilePaths ?
                QDir::current().relativeFilePath(fileName) : fileName);
    }

#define ADD(COLUMN_TYPE, ITEM_TYPE, PROPERTY) \
    if (item.type == Item::Type::ITEM_TYPE && \
            shouldSerializeColumn(item, COLUMN_TYPE)) \
        object[#PROPERTY] = toJsonValue(\
            static_cast<const ITEM_TYPE&>(item).PROPERTY);

    ADD_EACH_COLUMN_TYPE()
#undef ADD

    if (auto binding = castItem<Binding>(item)) {
        auto values = QJsonArray();
        for (auto i = 0; i < binding->valueCount; ++i) {
            auto value = QJsonObject();
#define ADD(COLUMN_TYPE, PROPERTY) \
            if (shouldSerializeColumn(item, COLUMN_TYPE)) \
                value[#PROPERTY] = toJsonValue( \
                    binding->values[static_cast<size_t>(i)].PROPERTY);

            ADD_EACH_BINDING_VALUE_COLUMN_TYPE()
#undef ADD
            values.append(value);
        }
        object["values"] = values;
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

bool SessionModel::shouldSerializeColumn(const Item &item,
    ColumnType column) const
{
    auto result = true;
    switch (item.type) {
        case Item::Type::Texture: {
            const auto &texture = static_cast<const Texture&>(item);
            auto kind = getKind(texture);
            result &= (column != TextureHeight || kind.dimensions > 1);
            result &= (column != TextureDepth || kind.dimensions > 2);
            result &= (column != TextureLayers || kind.array);
            result &= (column != TextureSamples || kind.multisample);
            result &= (column != TextureFlipY || kind.dimensions > 1);
            break;
        }

        case Item::Type::Binding: {
            const auto &binding = static_cast<const Binding&>(item);
            const auto uniform = (binding.bindingType == Binding::BindingType::Uniform);
            const auto image = (binding.bindingType == Binding::BindingType::Image);
            const auto sampler = (binding.bindingType == Binding::BindingType::Sampler);
            result &= (column != BindingEditor || uniform);
            result &= (column != BindingValueFields || uniform);
            result &= (column != BindingValueTextureId || image || sampler);
            result &= (column != BindingValueLevel || image);
            result &= (column != BindingValueLayered || image);
            result &= (column != BindingValueLayer || image);
            result &= (column != BindingValueMinFilter || sampler);
            result &= (column != BindingValueMagFilter || sampler);
            result &= (column != BindingValueWrapModeX || sampler);
            result &= (column != BindingValueWrapModeY || sampler);
            result &= (column != BindingValueWrapModeZ || sampler);
            result &= (column != BindingValueBorderColor || sampler);
            result &= (column != BindingValueComparisonFunc || sampler);
            result &= (column != BindingValueBufferId ||
                (binding.bindingType == Binding::BindingType::Buffer));
            result &= (column != BindingValueSubroutine ||
                (binding.bindingType == Binding::BindingType::Subroutine));
            break;
        }

        case Item::Type::Attachment: {
            const auto &attachment = static_cast<const Attachment&>(item);
            auto kind = TextureKind();
            if (auto texture = findItem<Texture>(attachment.textureId))
                kind = getKind(*texture);
            result &= (column != AttachmentLayered || kind.array);
            result &= (column != AttachmentLayer || kind.array);
            result &= (column != AttachmentBlendColorEq || kind.color);
            result &= (column != AttachmentBlendColorSource || kind.color);
            result &= (column != AttachmentBlendColorDest || kind.color);
            result &= (column != AttachmentBlendAlphaEq || kind.color);
            result &= (column != AttachmentBlendAlphaSource || kind.color);
            result &= (column != AttachmentBlendAlphaDest || kind.color);
            result &= (column != AttachmentColorWriteMask || kind.color);
            result &= (column != AttachmentDepthComparisonFunc || kind.depth);
            result &= (column != AttachmentDepthOffsetFactor || kind.depth);
            result &= (column != AttachmentDepthOffsetUnits || kind.depth);
            result &= (column != AttachmentDepthClamp || kind.depth);
            result &= (column != AttachmentDepthWrite || kind.depth);
            result &= (column != AttachmentStencilFrontComparisonFunc || kind.stencil);
            result &= (column != AttachmentStencilFrontReference || kind.stencil);
            result &= (column != AttachmentStencilFrontReadMask || kind.stencil);
            result &= (column != AttachmentStencilFrontFailOp || kind.stencil);
            result &= (column != AttachmentStencilFrontDepthFailOp || kind.stencil);
            result &= (column != AttachmentStencilFrontDepthPassOp || kind.stencil);
            result &= (column != AttachmentStencilFrontWriteMask || kind.stencil);
            result &= (column != AttachmentStencilBackComparisonFunc || kind.stencil);
            result &= (column != AttachmentStencilBackReference || kind.stencil);
            result &= (column != AttachmentStencilBackReadMask || kind.stencil);
            result &= (column != AttachmentStencilBackFailOp || kind.stencil);
            result &= (column != AttachmentStencilBackDepthFailOp || kind.stencil);
            result &= (column != AttachmentStencilBackDepthPassOp || kind.stencil);
            result &= (column != AttachmentStencilBackWriteMask || kind.stencil);
            break;
        }

        case Item::Type::Call: {
            const auto &call = static_cast<const Call&>(item);
            const auto kind = getKind(call);
            const auto callType = call.callType;
            result &= (column != CallProgramId || kind.draw || kind.compute);
            result &= (column != CallTargetId || kind.draw);
            result &= (column != CallVertexStreamId || kind.draw);
            result &= (column != CallPrimitiveType || kind.draw);
            result &= (column != CallPatchVertices || kind.drawPatches);
            result &= (column != CallIndexBufferId || kind.drawIndexed);
            result &= (column != CallCount || kind.drawDirect);
            result &= (column != CallFirst || kind.drawDirect);
            result &= (column != CallInstanceCount || kind.drawDirect);
            result &= (column != CallBaseInstance || kind.drawDirect);
            result &= (column != CallBaseVertex || (kind.drawDirect && kind.drawIndexed));
            result &= (column != CallIndirectBufferId || kind.drawIndirect);
            result &= (column != CallDrawCount || kind.drawIndirect);
            result &= (column != CallWorkGroupsX || kind.compute);
            result &= (column != CallWorkGroupsY || kind.compute);
            result &= (column != CallWorkGroupsZ || kind.compute);
            result &= (column != CallTextureId ||
                callType == Call::CallType::ClearTexture ||
                callType == Call::CallType::GenerateMipmaps);
            result &= (column != CallClearColor || callType == Call::CallType::ClearTexture);
            result &= (column != CallClearDepth || callType == Call::CallType::ClearTexture);
            result &= (column != CallClearStencil || callType == Call::CallType::ClearTexture);
            result &= (column != CallBufferId || callType == Call::CallType::ClearBuffer);
            break;
        }

        default:
            break;
    }
    return result;
}
