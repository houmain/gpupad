#include "SessionModel.h"
#include "FileDialog.h"
#include "SessionModelPriv.h"
#include <QAction>
#include <QDir>
#include <QIcon>
#include <QJsonDocument>
#include <QMimeData>
#include <QSaveFile>

SessionModel::SessionModel(QObject *parent) : SessionModelCore(parent)
{
    mTypeIcons[Item::Type::Session] = QIcon::fromTheme("folder");
    mTypeIcons[Item::Type::Group] = QIcon::fromTheme("folder");
    mTypeIcons[Item::Type::Buffer] = QIcon::fromTheme("x-office-spreadsheet");
    mTypeIcons[Item::Type::Block] = QIcon::fromTheme("format-justify-left");
    mTypeIcons[Item::Type::Field] = QIcon::fromTheme("mail-attachment");
    mTypeIcons[Item::Type::Texture] = QIcon::fromTheme("folder-pictures");
    mTypeIcons[Item::Type::Program] = QIcon::fromTheme("applications-system");
    mTypeIcons[Item::Type::Shader] =
        QIcon::fromTheme("accessories-text-editor");
    mTypeIcons[Item::Type::Binding] = QIcon::fromTheme("insert-text");
    mTypeIcons[Item::Type::Stream] =
        QIcon::fromTheme("media-playback-start-rtl");
    mTypeIcons[Item::Type::Attribute] = QIcon::fromTheme("mail-attachment");
    mTypeIcons[Item::Type::Target] = QIcon::fromTheme("video-display");
    mTypeIcons[Item::Type::Attachment] = QIcon::fromTheme("mail-attachment");
    mTypeIcons[Item::Type::Call] = QIcon::fromTheme("dialog-information");
    mTypeIcons[Item::Type::Script] =
        QIcon::fromTheme("accessories-text-editor");
    mTypeIcons[Item::Type::AccelerationStructure] =
        QIcon::fromTheme("zoom-fit-best");
    mTypeIcons[Item::Type::Instance] = QIcon::fromTheme("mail-attachment");
    mTypeIcons[Item::Type::Geometry] =
        QIcon::fromTheme("media-playback-start-rtl");

    connect(&undoStack(), &QUndoStack::cleanChanged, this,
        &SessionModel::undoStackCleanChanged);
}

SessionModel::~SessionModel()
{
    clear();
}

QList<QMetaObject::Connection> SessionModel::connectUndoActions(QAction *undo,
    QAction *redo)
{
    auto c = QList<QMetaObject::Connection>();
    undo->setEnabled(undoStack().canUndo());
    redo->setEnabled(undoStack().canRedo());
    c += connect(undo, &QAction::triggered, &undoStack(), &QUndoStack::undo);
    c += connect(redo, &QAction::triggered, &undoStack(), &QUndoStack::redo);
    c += connect(&undoStack(), &QUndoStack::canUndoChanged, undo,
        &QAction::setEnabled);
    c += connect(&undoStack(), &QUndoStack::canRedoChanged, redo,
        &QAction::setEnabled);
    return c;
}

bool SessionModel::isUndoStackClean()
{
    return undoStack().isClean();
}

void SessionModel::clearUndoStack()
{
    undoStack().clear();
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
    if (role == Qt::ForegroundRole) {
        if (mActiveItemIds.contains(item.id))
            return mActiveItemsColor;
        return QVariant();
    }

    if (role == Qt::ToolTipRole)
        if (auto fileItem = castItem<FileItem>(item))
            if (!FileDialog::isEmptyOrUntitled(fileItem->fileName))
                return fileItem->fileName;

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
    flags |= Qt::ItemIsUserCheckable;
    if (type != Item::Type::Session)
        flags |= Qt::ItemIsDragEnabled;
    switch (type) {
    case Item::Type::Root:
    case Item::Type::Session:
    case Item::Type::Group:
    case Item::Type::Buffer:
    case Item::Type::Block:
    case Item::Type::Texture:
    case Item::Type::Program:
    case Item::Type::Stream:
    case Item::Type::Target:
    case Item::Type::AccelerationStructure:
    case Item::Type::Instance:              flags |= Qt::ItemIsDropEnabled; break;
    default:                                break;
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
    const auto difference =
        (mActiveItemIds | itemIds) - (mActiveItemIds & itemIds);

    if (difference.empty())
        return;

    mActiveItemIds = std::move(itemIds);

    for (const auto &itemId : difference) {
        const auto index = getIndex(findItem(itemId), ColumnType::Name);
        Q_EMIT dataChanged(index, index, { Qt::ForegroundRole });
    }
}

void SessionModel::setActiveItemColor(QColor color)
{
    mActiveItemsColor = color;
}

QString SessionModel::getItemName(ItemId id) const
{
    if (auto item = findItem(id))
        return item->name;
    return {};
}

QString SessionModel::getFullItemName(ItemId id) const
{
    auto item = findItem(id);
    if (!item)
        return {};
    const auto fullwidthHyphenMinus = QChar(0xFF0D);
    auto name = item->name;
    const auto end = &sessionItem();
    for (item = item->parent; item && item != end; item = item->parent) {
        const auto sep = (item->type == Item::Type::Block
                ? QChar('.')
                : fullwidthHyphenMinus);
        name = item->name + sep + name;
    }
    return name;
}

QStringList SessionModel::mimeTypes() const
{
    return { QStringLiteral("text/plain"), QStringLiteral("text/uri-list") };
}

Qt::DropActions SessionModel::supportedDragActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::DropActions SessionModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QJsonArray SessionModel::generateJsonFromUrls(QModelIndex target,
    const QList<QUrl> &urls) const
{
    auto itemArray = QJsonArray();
    const auto addFileItem = [&](auto &item, const QUrl &url) {
        item.name = url.fileName();
        item.fileName = toNativeCanonicalFilePath(url.toLocalFile());

        auto object = QJsonObject();
        serialize(object, item, true);
        itemArray.append(object);
    };

    for (const auto &url : urls) {
        const auto fileName = url.toLocalFile();
        if (canContainType(target, Item::Type::Shader)
            && FileDialog::isShaderFileName(fileName)) {
            auto item = Shader();
            item.type = Item::Type::Shader;
            addFileItem(item, url);
        } else if (canContainType(target, Item::Type::Script)
            && FileDialog::isScriptFileName(fileName)) {
            auto item = Script();
            item.type = Item::Type::Script;
            addFileItem(item, url);
        } else if (canContainType(target, Item::Type::Texture)
            && (FileDialog::isTextureFileName(fileName)
                || FileDialog::isVideoFileName(fileName))) {
            auto item = Texture();
            item.type = Item::Type::Texture;
            addFileItem(item, url);
        } else if (canContainType(target, Item::Type::Buffer)
            && !FileDialog::isShaderFileName(fileName)
            && !FileDialog::isScriptFileName(fileName)
            && !FileDialog::isTextureFileName(fileName)
            && !FileDialog::isVideoFileName(fileName)) {
            auto item = Buffer();
            item.type = Item::Type::Buffer;
            addFileItem(item, url);
        }
    }
    return itemArray;
}

QJsonArray SessionModel::parseDraggedJson(QModelIndex target,
    const QMimeData *data) const
{
    if (data->hasUrls()) {
        mDraggedJson = generateJsonFromUrls(target, data->urls());
        return mDraggedJson;
    }

    auto text = data->text();
    if (text != mDraggedText) {
        mDraggedText = text;

        auto document = QJsonDocument::fromJson(text.toUtf8());
        mDraggedJson = document.isNull() ? QJsonArray()
            : document.isArray()         ? document.array()
                                         : QJsonArray({ document.object() });
    }
    return mDraggedJson;
}

bool SessionModel::canDropMimeData(const QMimeData *data, Qt::DropAction action,
    int row, int column, const QModelIndex &parent) const
{
    if (!QAbstractItemModel::canDropMimeData(data, action, row, column, parent))
        return false;

    const auto jsonArray = parseDraggedJson(parent, data);
    for (const QJsonValue &value : jsonArray) {
        auto ok = false;
        const auto object = value.toObject();
        const auto typeName = object["type"].toString();
        const auto type = getTypeByName(typeName, ok);
        if (!ok || !canContainType(parent, type))
            return false;
    }
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

    // try to drop items in session
    auto target = sessionItemIndex();
    if (!canDropMimeData(&data, Qt::CopyAction, rowCount(), 0, target)) {
        // try to replace session with dropped session
        if (!canDropMimeData(&data, Qt::CopyAction, rowCount(), 0, {}))
            return false;
        deleteItem(QModelIndex());
        target = {};
    }

    undoStack().clear();
    undoStack().setUndoLimit(1);

    dropMimeData(&data, Qt::CopyAction, 0, 0, target);

    undoStack().clear();
    undoStack().setUndoLimit(0);
    return true;
}

bool SessionModel::save(const QString &fileName)
{
    QDir::setCurrent(QFileInfo(fileName).path());
    std::unique_ptr<QMimeData> mime(mimeData({ QModelIndex() }));
    if (!mime)
        return false;

    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(mime->text().toUtf8());
    if (!file.commit())
        return false;

    undoStack().setClean();
    return true;
}

QMimeData *SessionModel::mimeData(const QModelIndexList &indexes) const
{
    mDraggedIndices = indexes;
    mDraggedUntitledFileNames.clear();

    auto jsonArray = getJson(indexes);
    auto document = (jsonArray.size() != 1
            ? QJsonDocument(jsonArray)
            : QJsonDocument(jsonArray.first().toObject()));
    auto data = new QMimeData();
    data->setText(document.toJson());
    return data;
}

bool SessionModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
    int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(column);
    if (action == Qt::IgnoreAction)
        return true;

    auto jsonArray = parseDraggedJson(parent, data);
    if (jsonArray.empty())
        return false;

    if (row < 0)
        row = rowCount(parent);

    beginUndoMacro("Move");

    if (action == Qt::MoveAction && !mDraggedIndices.empty()) {
        std::stable_sort(mDraggedIndices.begin(), mDraggedIndices.end(),
            [](const auto &a, const auto &b) { return a.row() > b.row(); });
        for (const auto &index : std::as_const(mDraggedIndices)) {
            if (index.parent() == parent && index.row() < row)
                --row;
            deleteItem(index);
        }
    }

    dropJson(jsonArray, row, parent, false);
    mDraggedIndices.clear();

    endUndoMacro();
    return true;
}

QJsonArray SessionModel::getJson(const QModelIndexList &indexes) const
{
    auto itemArray = QJsonArray();
    if (indexes.size() == 1 && !indexes.first().isValid()) {
        for (const Item *item : getItem({}).items) {
            auto object = QJsonObject();
            serialize(object, *item, true);
            itemArray.append(object);
        }
    } else {
        for (const QModelIndex &index : indexes) {
            auto object = QJsonObject();
            serialize(object, getItem(index), false);
            itemArray.append(object);
        }
    }
    return itemArray;
}

void SessionModel::dropJson(const QJsonArray &jsonArray, int row,
    const QModelIndex &parent, bool updateExisting)
{
    for (const QJsonValue &value : jsonArray)
        deserialize(value.toObject(), parent, row++, updateExisting);

    // fixup item references
    const auto keys = mDroppedIdsReplaced.keys();
    for (ItemId prevId : keys)
        for (const QModelIndex &reference : std::as_const(mDroppedReferences))
            if (data(reference).toInt() == prevId)
                setData(reference, mDroppedIdsReplaced[prevId]);
    mDroppedIdsReplaced.clear();
    mDroppedReferences.clear();
}

void SessionModel::deserialize(const QJsonObject &object,
    const QModelIndex &parent, int row, bool updateExisting)
{
    auto id = (object.contains("id") ? object["id"].toInt() : getNextItemId());

    auto existingItem = findItem(id);
    auto untitledFileName = QString();
    if (existingItem && !updateExisting) {
        // generate new id when it collides with an existing item
        const auto prevId = std::exchange(id, getNextItemId());
        mDroppedIdsReplaced.insert(prevId, id);
        existingItem = nullptr;

        if (mDraggedUntitledFileNames.contains(prevId))
            untitledFileName = mDraggedUntitledFileNames[prevId];
    } else if (mDraggedUntitledFileNames.contains(id)) {
        untitledFileName = mDraggedUntitledFileNames[id];
    }

    auto ok = true;
    const auto type = (existingItem ? existingItem->type
            : object.contains("type")
            ? getTypeByName(object["type"].toString(), ok)
            : getDefaultChildType(parent));
    if (!ok)
        return;

    const auto isDynamicGroup =
        (type == Item::Type::Group && object["dynamic"].toBool());

    const auto index = (existingItem
            ? getIndex(existingItem, ColumnType::Name)
            : insertItem(type, parent, row, id, isDynamicGroup));

    // preserve untitled filenames when dragging/copying
    if (!untitledFileName.isEmpty())
        setData(getIndex(index, FileName), untitledFileName);

    const auto dropColumn = [&](const QString &property,
                                const QModelIndex &index,
                                const QVariant &value) {
        if (property.endsWith("Id")) {
            // reference to an already patched id?
            auto it = mDroppedIdsReplaced.find(value.toInt());
            if (it != mDroppedIdsReplaced.end()) {
                setData(index, it.value());
                return;
            }
            // otherwise remember reference for fixup
            mDroppedReferences.append(index);
        }
        setData(index, value);
    };

    const auto keys = object.keys();
    for (const QString &property : keys) {
        auto value = object[property].toVariant();

        if (property == "name") {
            setData(getIndex(index, Name), value);
        } else if (property == "fileName") {
            const auto absolutePath =
                QDir::current().absoluteFilePath(value.toString());
            setData(getIndex(index, FileName),
                toNativeCanonicalFilePath(absolutePath));
        } else if (property == "target") {
            // TODO: remove, added for backward compatibility
            if (value == "Target2DMultisample")
                value = "Target2D";
            if (value == "Target2DMultisampleArray")
                value = "Target2DArray";
            dropColumn(property, getIndex(index, TextureTarget), value);
        } else {
            static const auto sPropertyNameColumnTypeMap =
                std::map<std::pair<Item::Type, QString>, ColumnType>{
#define ADD(COLUMN_TYPE, ITEM_TYPE, PROPERTY) \
    { { Item::Type::ITEM_TYPE, #PROPERTY }, COLUMN_TYPE },
                    ADD_EACH_COLUMN_TYPE()
#undef ADD
                };
            const auto it = sPropertyNameColumnTypeMap.find({ type, property });
            if (it != sPropertyNameColumnTypeMap.end())
                dropColumn(property, getIndex(index, it->second), value);
        }
    }

    auto items = object["items"].toArray();
    if (!items.isEmpty())
        for (const QJsonValue &value : std::as_const(items))
            deserialize(value.toObject(), index, -1, updateExisting);
}

void SessionModel::serialize(QJsonObject &object, const Item &item,
    bool relativeFilePaths) const
{
    object["type"] = getTypeName(item.type);
    object["id"] = item.id;
    if (auto fileItem = castItem<FileItem>(item)) {
        const auto &fileName = fileItem->fileName;
        if (!fileName.isEmpty()) {
            if (FileDialog::isUntitled(fileName)) {
                mDraggedUntitledFileNames[item.id] = fileName;
            } else {
                object["fileName"] = (relativeFilePaths
                        ? QDir::fromNativeSeparators(
                            QDir::current().relativeFilePath(fileName))
                        : fileName);
            }
        }
    }
    if (!object.contains("fileName"))
        object["name"] = item.name;

#define ADD(COLUMN_TYPE, ITEM_TYPE, PROPERTY)        \
    if (item.type == Item::Type::ITEM_TYPE           \
        && shouldSerializeColumn(item, COLUMN_TYPE)) \
        object[#PROPERTY] =                          \
            toJsonValue(static_cast<const ITEM_TYPE &>(item).PROPERTY);

    ADD_EACH_COLUMN_TYPE()
#undef ADD

    if (!item.items.empty() && !isDynamicGroup(item)) {
        auto items = QJsonArray();
        for (const Item *item : item.items) {
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
    case Item::Type::Session: {
        const auto &session = static_cast<const Session &>(item);
        const auto hasVulkanRenderer = (session.renderer == "Vulkan");
        const auto hasShaderCompiler =
            (!session.shaderCompiler.isEmpty() || hasVulkanRenderer);
        result &= (column != SessionFlipViewport || hasVulkanRenderer);
        result &= (column != SessionReverseCulling || hasVulkanRenderer);
        result &= (column != SessionShaderCompiler || !hasVulkanRenderer);
        result &= (column != SessionAutoMapBindings || hasShaderCompiler);
        result &= (column != SessionAutoMapLocations || hasShaderCompiler);
        result &= (column != SessionAutoSampledTextures || hasShaderCompiler);
        result &= (column != SessionVulkanRulesRelaxed || hasShaderCompiler);
        result &= (column != SessionSpirvVersion
            || (hasShaderCompiler && session.spirvVersion != 0));
        break;
    }

    case Item::Type::Shader: {
        const auto &shader = static_cast<const Shader &>(item);
        result &= (column != ShaderEntryPoint
            || (shader.language != Shader::Language::GLSL));
        result &= (column != ShaderPreamble || !shader.preamble.isEmpty());
        result &=
            (column != ShaderIncludePaths || !shader.includePaths.isEmpty());
        break;
    }

    case Item::Type::Texture: {
        const auto &texture = static_cast<const Texture &>(item);
        auto kind = getKind(texture);
        result &=
            (column != TextureHeight || (kind.dimensions > 1 && !kind.cubeMap));
        result &= (column != TextureDepth || kind.dimensions > 2);
        result &= (column != TextureLayers || kind.array);
        break;
    }

    case Item::Type::Binding: {
        const auto &binding = static_cast<const Binding &>(item);
        const auto uniform =
            (binding.bindingType == Binding::BindingType::Uniform);
        const auto sampler =
            (binding.bindingType == Binding::BindingType::Sampler);
        const auto image = (binding.bindingType == Binding::BindingType::Image);
        const auto textureBuffer =
            (binding.bindingType == Binding::BindingType::TextureBuffer);
        const auto buffer =
            (binding.bindingType == Binding::BindingType::Buffer);
        const auto block =
            (binding.bindingType == Binding::BindingType::BufferBlock);
        const auto subroutine =
            (binding.bindingType == Binding::BindingType::Subroutine);
        result &= (column != BindingEditor || uniform);
        result &= (column != BindingValues || uniform);
        result &= (column != BindingTextureId || image || sampler);
        result &= (column != BindingLevel || image);
        result &= (column != BindingLayer || image);
        result &= (column != BindingImageFormat || image || textureBuffer);
        result &= (column != BindingMinFilter || sampler);
        result &= (column != BindingMagFilter || sampler);
        result &= (column != BindingAnisotropic || sampler);
        result &= (column != BindingWrapModeX || sampler);
        result &= (column != BindingWrapModeY || sampler);
        result &= (column != BindingWrapModeZ || sampler);
        result &= (column != BindingBorderColor || sampler);
        result &= (column != BindingComparisonFunc || sampler);
        result &= (column != BindingBufferId || buffer || textureBuffer);
        result &= (column != BindingBlockId || block);
        result &= (column != BindingSubroutine || subroutine);
        break;
    }

    case Item::Type::Attachment: {
        const auto &attachment = static_cast<const Attachment &>(item);
        auto kind = TextureKind();
        if (auto texture = findItem<Texture>(attachment.textureId))
            kind = getKind(*texture);
        result &= (column != AttachmentLayer || kind.array);
        result &= (column != AttachmentBlendColorEq || kind.color);
        result &= (column != AttachmentBlendColorSource || kind.color);
        result &= (column != AttachmentBlendColorDest || kind.color);
        result &= (column != AttachmentBlendAlphaEq || kind.color);
        result &= (column != AttachmentBlendAlphaSource || kind.color);
        result &= (column != AttachmentBlendAlphaDest || kind.color);
        result &= (column != AttachmentColorWriteMask || kind.color);
        result &= (column != AttachmentDepthComparisonFunc || kind.depth);
        result &= (column != AttachmentDepthOffsetSlope || kind.depth);
        result &= (column != AttachmentDepthOffsetConstant || kind.depth);
        result &= (column != AttachmentDepthClamp || kind.depth);
        result &= (column != AttachmentDepthWrite || kind.depth);
        result &=
            (column != AttachmentStencilFrontComparisonFunc || kind.stencil);
        result &= (column != AttachmentStencilFrontReference || kind.stencil);
        result &= (column != AttachmentStencilFrontReadMask || kind.stencil);
        result &= (column != AttachmentStencilFrontFailOp || kind.stencil);
        result &= (column != AttachmentStencilFrontDepthFailOp || kind.stencil);
        result &= (column != AttachmentStencilFrontDepthPassOp || kind.stencil);
        result &= (column != AttachmentStencilFrontWriteMask || kind.stencil);
        result &=
            (column != AttachmentStencilBackComparisonFunc || kind.stencil);
        result &= (column != AttachmentStencilBackReference || kind.stencil);
        result &= (column != AttachmentStencilBackReadMask || kind.stencil);
        result &= (column != AttachmentStencilBackFailOp || kind.stencil);
        result &= (column != AttachmentStencilBackDepthFailOp || kind.stencil);
        result &= (column != AttachmentStencilBackDepthPassOp || kind.stencil);
        result &= (column != AttachmentStencilBackWriteMask || kind.stencil);
        break;
    }

    case Item::Type::Call: {
        const auto &call = static_cast<const Call &>(item);
        const auto kind = getKind(call);
        const auto callType = call.callType;
        result &= (column != CallProgramId || kind.draw || kind.compute
            || kind.trace);
        result &= (column != CallTargetId || kind.draw);
        result &= (column != CallVertexStreamId || (kind.draw && !kind.mesh));
        result &= (column != CallPrimitiveType || (kind.draw && !kind.mesh));
        result &= (column != CallPatchVertices || kind.patches);
        result &= (column != CallIndexBufferBlockId || kind.indexed);
        result &= (column != CallCount
            || ((kind.draw && !kind.mesh) && !kind.indirect));
        result &= (column != CallFirst
            || (kind.draw && !kind.mesh && !kind.indirect));
        result &= (column != CallInstanceCount
            || (kind.draw && !kind.mesh && !kind.indirect));
        result &= (column != CallBaseInstance
            || (kind.draw && !kind.mesh && !kind.indirect));
        result &= (column != CallBaseVertex
            || (kind.draw && kind.indexed && !kind.mesh && !kind.indirect));
        result &= (column != CallIndirectBufferBlockId || kind.indirect);
        result &= (column != CallDrawCount || (kind.draw && kind.indirect));
        result &= (column != CallWorkGroupsX
            || ((kind.compute || kind.mesh || kind.trace) && !kind.indirect));
        result &= (column != CallWorkGroupsY
            || ((kind.compute || kind.mesh || kind.trace) && !kind.indirect));
        result &= (column != CallWorkGroupsZ
            || ((kind.compute || kind.mesh || kind.trace) && !kind.indirect));
        result &= (column != CallTextureId
            || callType == Call::CallType::ClearTexture
            || callType == Call::CallType::CopyTexture
            || callType == Call::CallType::SwapTextures);
        result &= (column != CallFromTextureId
            || callType == Call::CallType::CopyTexture
            || callType == Call::CallType::SwapTextures);
        result &= (column != CallClearColor
            || callType == Call::CallType::ClearTexture);
        result &= (column != CallClearDepth
            || callType == Call::CallType::ClearTexture);
        result &= (column != CallClearStencil
            || callType == Call::CallType::ClearTexture);
        result &= (column != CallBufferId
            || callType == Call::CallType::ClearBuffer
            || callType == Call::CallType::CopyBuffer
            || callType == Call::CallType::SwapBuffers);
        result &= (column != CallFromBufferId
            || callType == Call::CallType::CopyBuffer
            || callType == Call::CallType::SwapBuffers);
        result &= (column != CallAccelerationStructureId
            || callType == Call::CallType::TraceRays);
        break;
    }

    case Item::Type::Script: {
        const auto &script = static_cast<const Script &>(item);
        result &= (column != FileName || !script.fileName.isEmpty());
        break;
    }

    case Item::Type::Target: {
        const auto &target = static_cast<const Target &>(item);
        const auto hasAttachments = !target.items.empty();
        result &= (column != TargetDefaultWidth || !hasAttachments);
        result &= (column != TargetDefaultHeight || !hasAttachments);
        result &= (column != TargetDefaultLayers || !hasAttachments);
        result &= (column != TargetDefaultSamples || !hasAttachments);
        break;
    }

    case Item::Type::Geometry: {
        const auto &geometry = static_cast<const Geometry &>(item);
        const auto hasIndices =
            (geometry.geometryType == Geometry::GeometryType::Triangles);
        result &= (column != GeometryIndexBufferBlockId || hasIndices);
        break;
    }

    default: break;
    }
    return result;
}
