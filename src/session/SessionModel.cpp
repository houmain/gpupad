#include "SessionModel.h"
#include "FileDialog.h"
#include "SessionModelPriv.h"
#include <QAction>
#include <QDir>
#include <QIcon>
#include <QMimeData>
#include <QSaveFile>
#include <QUrl>

namespace {
    void materializeSessionInterfaces(JsonArray &items, bool topLevel,
        JsonArray &topLevelInterfaceItems)
    {
        auto remainingItems = JsonArray{ };
        for (auto value : items) {
            if (!value.is_object()) {
                remainingItems.push_back(std::move(value));
                continue;
            }

            auto object = *jsonObject(value);
            if (const auto childrenValue = findJsonValue(object, "items");
                childrenValue && childrenValue->is_array()) {
                auto children = *jsonArray(*childrenValue);
                materializeSessionInterfaces(children, false,
                    topLevelInterfaceItems);
                if (children.empty())
                    object.erase("items");
                else
                    object["items"] = std::move(children);
            }

            const auto interface =
                jsonValue<QString>(object, "sessionInterface");
            if (!interface.isEmpty() && interface != "Internal") {
                object["componentInterface"] = interface;
                object.erase("sessionInterface");
                if (!topLevel) {
                    topLevelInterfaceItems.push_back(std::move(object));
                    continue;
                }
            }
            remainingItems.push_back(std::move(object));
        }
        items = std::move(remainingItems);
    }

    template <typename T>
    QVariantMap propertyInfo(const QString &name)
    {
        auto result = QVariantMap{
            { "name", name },
        };
        auto type = QString();
        if constexpr (std::is_enum_v<T>) {
            type = "enum";
            const auto meta = QMetaEnum::fromType<T>();
            auto values = QStringList();
            for (auto i = 0; i < meta.keyCount(); ++i)
                values.append(QString::fromLatin1(meta.key(i)));
            result["values"] = values;
        } else if constexpr (std::is_same_v<T, bool>) {
            type = "boolean";
        } else if constexpr (std::is_integral_v<T>) {
            type = name.endsWith("Id") ? "reference" : "integer";
            result["unsigned"] = std::is_unsigned_v<T>;
        } else if constexpr (std::is_floating_point_v<T>) {
            type = "number";
        } else if constexpr (std::is_same_v<T, QString>) {
            type = "string";
        } else if constexpr (std::is_same_v<T, QColor>) {
            type = "color";
        } else if constexpr (std::is_same_v<T, QStringList>) {
            type = "array";
        } else {
            type = "object";
        }
        result["type"] = type;
        return result;
    }
} // namespace

QVariantList SessionModel::itemProperties(const Item &item) const
{
    auto result = QVariantList{
        propertyInfo<decltype(item.id)>("id"),
        propertyInfo<decltype(item.type)>("type"),
        propertyInfo<decltype(item.name)>("name"),
        propertyInfo<decltype(item.custom)>("custom"),
        propertyInfo<decltype(item.sessionInterface)>("sessionInterface"),
        propertyInfo<decltype(item.componentInterface)>("componentInterface"),
        propertyInfo<decltype(item.inputItemId)>("inputItemId"),
    };
    if (const auto fileItem = castItem<FileItem>(&item))
        result.append(propertyInfo<decltype(fileItem->fileName)>("fileName"));
    result.append(QVariantMap{ { "name", "items" }, { "type", "array" } });
#define ADD(COLUMN_TYPE, ITEM_TYPE, PROPERTY) \
    if (item.type == Item::Type::ITEM_TYPE)   \
        result.append(propertyInfo<decltype(ITEM_TYPE::PROPERTY)>(#PROPERTY));
    ADD_EACH_COLUMN_TYPE()
#undef ADD
    return result;
}

SessionModel::SessionModel(QObject *parent) : SessionModelCore(parent)
{
    mTypeIcons[Item::Type::Session] = QIcon::fromTheme("folder");
    mTypeIcons[Item::Type::Group] = QIcon::fromTheme("folder");
    mTypeIcons[Item::Type::Component] = QIcon::fromTheme("dialog-information");
    mTypeIcons[Item::Type::Buffer] = QIcon::fromTheme("x-office-spreadsheet");
    mTypeIcons[Item::Type::Block] = QIcon::fromTheme("format-justify-left");
    mTypeIcons[Item::Type::Field] = QIcon::fromTheme("mail-attachment");
    mTypeIcons[Item::Type::Texture] = QIcon::fromTheme("folder-pictures");
    mTypeIcons[Item::Type::Program] = QIcon::fromTheme("applications-system");
    mTypeIcons[Item::Type::Shader] =
        QIcon::fromTheme("accessories-text-editor");
    mTypeIcons[Item::Type::ShaderTable] =
        QIcon::fromTheme("applications-system");
    mTypeIcons[Item::Type::Record] = QIcon::fromTheme("mail-attachment");
    mTypeIcons[Item::Type::Binding] = QIcon::fromTheme("insert-text");
    mTypeIcons[Item::Type::Stream] =
        QIcon::fromTheme("media-playback-start-rtl");
    mTypeIcons[Item::Type::Attribute] = QIcon::fromTheme("mail-attachment");
    mTypeIcons[Item::Type::Target] = QIcon::fromTheme("video-display");
    mTypeIcons[Item::Type::Attachment] = QIcon::fromTheme("mail-attachment");
    mTypeIcons[Item::Type::Call] = QIcon::fromTheme("dialog-information");
    mTypeIcons[Item::Type::Script] =
        QIcon::fromTheme("accessories-text-editor");
    mTypeIcons[Item::Type::GeometrySet] = QIcon::fromTheme("zoom-fit-best");
    mTypeIcons[Item::Type::Geometry] =
        QIcon::fromTheme("media-playback-start-rtl");
    mTypeIcons[Item::Type::AccelerationStructure] =
        QIcon::fromTheme("zoom-fit-best");
    mTypeIcons[Item::Type::Instance] = QIcon::fromTheme("mail-attachment");

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

bool SessionModel::isLoading()
{
    return (undoStack().undoLimit() == 1);
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
    auto type = getItemType(index);
    auto flags = Qt::ItemFlags();
    flags |= Qt::ItemIsSelectable;
    flags |= Qt::ItemIsEnabled;
    flags |= Qt::ItemIsUserCheckable;
    if (!isComponentContent(index)) {
        flags |= Qt::ItemIsEditable;
        flags |= Qt::ItemIsDragEnabled;
    }
    switch (type) {
    case Item::Type::Root:
    case Item::Type::Session:
    case Item::Type::Group:
    case Item::Type::Buffer:
    case Item::Type::Block:
    case Item::Type::Texture:
    case Item::Type::Program:
    case Item::Type::ShaderTable:
    case Item::Type::Record:
    case Item::Type::Stream:
    case Item::Type::Target:
    case Item::Type::AccelerationStructure:
    case Item::Type::Instance:
    case Item::Type::GeometrySet:           flags |= Qt::ItemIsDropEnabled; break;
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
    // do not allow to move Session or from Component
    if (!parent.isValid() || isComponentOrContent(parent))
        return false;

    SessionModelCore::removeRows(row, count, parent);
    mDraggedIndices.clear();
    return true;
}

void SessionModel::deleteItem(const QModelIndex &index)
{
    if (isComponentContent(index))
        return;
    SessionModelCore::deleteItem(index);
}

bool SessionModel::isComponentContent(const QModelIndex &index) const
{
    for (auto parent = index.parent(); parent.isValid();
        parent = parent.parent())
        if (getItemType(parent) == Item::Type::Component)
            return true;
    return false;
}

bool SessionModel::isComponentOrContent(const QModelIndex &index) const
{
    return index.isValid()
        && (getItemType(index) == Item::Type::Component
            || isComponentContent(index));
}

void SessionModel::setActiveItems(QSet<ItemId> itemIds)
{
    const auto difference = (mActiveItemIds | itemIds)
        - (mActiveItemIds & itemIds);
    if (difference.empty())
        return;

    mActiveItemIds = std::move(itemIds);

    for (const auto &itemId : difference) {
        const auto index = getIndex(findItem(itemId));
        if (index.isValid())
            Q_EMIT dataChanged(index, index, { Qt::ForegroundRole });
    }
}

void SessionModel::setActiveItemColor(QColor color)
{
    mActiveItemsColor = color;
}

const Item *SessionModel::findItemByPath(const QModelIndex &originIndex,
    const QString &path) const
{
    const auto parts = path.split('/');
    auto index = originIndex;
    for (const auto &part : parts) {
        index = findChildByName(index, part);
        if (!index.isValid())
            return nullptr;
    }
    return &getItem(index);
}

QString SessionModel::getItemName(ItemId id) const
{
    if (auto item = findItem(id))
        return item->name;
    return { };
}

QString SessionModel::getFullItemName(ItemId id) const
{
    auto item = findItem(id);
    if (!item)
        return { };
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

bool SessionModel::itemCanBeInSessionInterface(const QModelIndex &index) const
{
    const auto itemType = getItemType(index);
    switch (itemType) {
    case Item::Type::Buffer:
    case Item::Type::Texture:
    case Item::Type::Program:
    case Item::Type::ShaderTable:
    case Item::Type::Stream:
    case Item::Type::Target:
    case Item::Type::GeometrySet:
    case Item::Type::AccelerationStructure:
        return true;
        // these types cannot be moved to top level without changing behavior
    case Item::Type::Group:
    case Item::Type::Binding:
    case Item::Type::Call:
    case Item::Type::Script:
        return (getItemType(index.parent()) == Item::Type::Session);
    default: break;
    }
    return false;
}

bool SessionModel::isItemVisibleInScope(const Item &current,
    const Item &candidate) const
{
    const auto findComponent = [](const Item &item, const Item **child) {
        auto *component = static_cast<const Component *>(nullptr);
        auto *componentChild = &item;
        for (auto *parent = item.parent; parent;
            componentChild = parent, parent = parent->parent) {
            if (parent->type == Item::Type::Component) {
                component = static_cast<const Component *>(parent);
                break;
            }
        }
        if (child)
            *child = component ? componentChild : nullptr;
        return component;
    };

    const auto *currentComponent = findComponent(current, nullptr);
    const auto *componentChild = static_cast<const Item *>(nullptr);
    const auto *component = findComponent(candidate, &componentChild);
    if (!component)
        return true;
    if (component != currentComponent)
        return componentChild->componentInterface == Item::Interface::Output;
    const auto currentIsInput = current.parent == currentComponent
        && current.componentInterface == Item::Interface::Input;
    return !currentIsInput
        || componentChild->componentInterface == Item::Interface::Output;
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

JsonArray SessionModel::generateJsonFromUrls(QModelIndex target,
    const QList<QUrl> &urls) const
{
    auto itemArray = JsonArray();
    const auto addFileItem = [&](auto &item, const QUrl &url) {
        item.name = url.fileName();
        item.fileName = toNativeCanonicalFilePath(url.toLocalFile());

        auto object = JsonObject();
        serialize(object, item, true);
        itemArray.push_back(object);
    };

    for (const auto &url : urls) {
        const auto fileName = url.toLocalFile();
        if (canContainType(target, Item::Type::Shader)
            && FileDialog::isShaderFileName(fileName)) {
            auto item = Shader();
            item.type = Item::Type::Shader;
            const auto sourceType =
                deduceSourceType(SourceType::PlainText, fileName, "");
            item.shaderType = getShaderType(sourceType);
            addFileItem(item, url);
        } else if (canContainType(target, Item::Type::Script)
            && FileDialog::isScriptFileName(fileName)) {
            auto item = Script();
            item.type = Item::Type::Script;
            addFileItem(item, url);
        } else if (canContainType(target, Item::Type::Texture)
            && (FileDialog::isTextureFileName(fileName)
                || FileDialog::isMediaFileName(fileName))) {
            auto item = Texture();
            item.type = Item::Type::Texture;
            if (FileDialog::isAudioFileName(fileName))
                item.sourceType = Texture::SourceType::AudioSpectrum;
            else if (FileDialog::isMediaFileName(fileName))
                item.sourceType = Texture::SourceType::Video;
            initializeTextureSource(item);
            addFileItem(item, url);
        } else if (canContainType(target, Item::Type::Buffer)
            && !FileDialog::isShaderFileName(fileName)
            && !FileDialog::isScriptFileName(fileName)
            && !FileDialog::isTextureFileName(fileName)
            && !FileDialog::isMediaFileName(fileName)) {
            auto item = Buffer();
            item.type = Item::Type::Buffer;
            addFileItem(item, url);
        }
    }
    return itemArray;
}

JsonArray SessionModel::parseDraggedJson(QModelIndex &target,
    const QMimeData *data) const
{
    if (data->hasUrls()) {
        if (!target.isValid())
            target = sessionItemIndex();
        mDraggedJson = generateJsonFromUrls(target, data->urls());
        return mDraggedJson;
    }

    auto text = data->text();
    if (text != mDraggedText) {
        mDraggedText = text;

        const auto document = parseJson(text);
        mDraggedJson = document.is_array() ? *jsonArray(document)
            : document.is_object()         ? JsonArray{ *jsonObject(document) }
                                           : JsonArray();
    }
    return mDraggedJson;
}

bool SessionModel::canDropMimeData(const QMimeData *data, Qt::DropAction action,
    int row, int column, const QModelIndex &parent) const
{
    if (!QAbstractItemModel::canDropMimeData(data, action, row, column, parent))
        return false;

    auto target = parent;
    const auto jsonArray = parseDraggedJson(target, data);
    if (jsonArray.empty())
        return false;
    if (isComponentOrContent(target))
        return false;
    if (action == Qt::MoveAction)
        for (const auto &index : mDraggedIndices)
            if (isComponentContent(index))
                return false;

    for (const JsonValue &value : jsonArray) {
        const auto object = jsonObject(value);
        if (!object)
            return false;
        auto ok = false;
        const auto typeName = jsonValue<QString>(*object, "type");
        const auto type = getItemTypeByName(typeName, &ok);
        if (!ok || !canContainType(target, type))
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
        if (!canDropMimeData(&data, Qt::CopyAction, rowCount(), 0, { }))
            return false;
        deleteItem(QModelIndex());
        target = { };
    }

    undoStack().clear();
    undoStack().setUndoLimit(1);

    dropMimeData(&data, Qt::CopyAction, 0, 0, target);

    undoStack().clear();
    undoStack().setUndoLimit(0);
    return true;
}

QModelIndex SessionModel::insertComponent(const QString &fileName_,
    const QModelIndex &parent, int row)
{
    const auto fileName = toNativeCanonicalAbsoluteFilePath(fileName_);

    const auto previousDirectory = QDir::currentPath();
    auto sourceModel = SessionModel();
    const auto loaded = sourceModel.load(fileName);
    QDir::setCurrent(previousDirectory);
    if (!loaded)
        return QModelIndex();

    auto sourceIndices = QModelIndexList{ };
    const auto sourceSessionIndex = sourceModel.sessionItemIndex();
    for (auto i = 0; i < sourceModel.rowCount(sourceSessionIndex); ++i)
        sourceIndices.append(sourceModel.index(i, 0, sourceSessionIndex));
    auto sourceItems = sourceModel.getJson(sourceIndices);
    // move nested interface items to the Component's top level
    auto topLevelInterfaceItems = JsonArray{ };
    materializeSessionInterfaces(sourceItems, true, topLevelInterfaceItems);
    sourceItems.insert(sourceItems.end(),
        std::make_move_iterator(topLevelInterfaceItems.begin()),
        std::make_move_iterator(topLevelInterfaceItems.end()));

    beginUndoMacro(tr("Add Component"));
    const auto componentIndex = insertItem(Item::Type::Component, parent, row);
    if (componentIndex.isValid()) {
        setField(componentIndex, Name, FileDialog::getFileTitle(fileName));
        setField(componentIndex, ComponentSource, fileName);
        dropJson(sourceItems, 0, componentIndex, DropMode::ComponentImport);
    }
    endUndoMacro();

    return componentIndex;
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
    auto document =
        (jsonArray.size() != 1 ? JsonValue(jsonArray) : jsonArray.front());
    auto data = new QMimeData();
    data->setText(serializeJson(document));
    return data;
}

bool SessionModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
    int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(column);
    if (action == Qt::IgnoreAction)
        return true;
    if (!canDropMimeData(data, action, row, column, parent))
        return false;

    auto target = parent;
    auto jsonArray = parseDraggedJson(target, data);
    if (jsonArray.empty())
        return false;

    if (row < 0)
        row = rowCount(target);

    beginUndoMacro("Move");

    if (action == Qt::MoveAction && !mDraggedIndices.empty()) {
        std::stable_sort(mDraggedIndices.begin(), mDraggedIndices.end(),
            [](const auto &a, const auto &b) { return a.row() > b.row(); });
        for (const auto &index : std::as_const(mDraggedIndices)) {
            if (index.parent() == target && index.row() < row)
                --row;
            deleteItem(index);
        }
    }

    // do not allow to drop another Sessioon
    if (!target.isValid() && sessionItemIndex().isValid())
        return false;

    dropJson(jsonArray, row, target, DropMode::Insert);
    mDraggedIndices.clear();

    endUndoMacro();
    return true;
}

void SessionModel::clear()
{
    mActiveItemIds.clear();
    SessionModelCore::clear();
}

QString SessionModel::ensureFileName(const FileItem &item, QString hint)
{
    auto fileName = item.fileName;
    if (fileName.isEmpty())
        fileName = std::move(hint);
    if (fileName.isEmpty()) {
        if (!FileDialog::getFileExtension(item.name).isEmpty())
            fileName = toNativeCanonicalAbsoluteFilePath(item.name);
        else
            fileName = FileDialog::generateNextUntitledFileName(item.name);
    }
    setField(&item, FileName, fileName);
    return fileName;
}

QSet<ItemId> SessionModel::resolveComponentInputs()
{
    auto referencedItems = QSet<ItemId>{ };
    forEachItem<Component>(
        [this, &referencedItems](const Component &component) {
            auto replacements = QMap<ItemId, ItemId>{ };
            for (const auto *item : component.items) {
                if (const auto *replacement = resolveComponentInput(item);
                    replacement && replacement != item) {
                    replacements.insert(item->id, replacement->id);
                    referencedItems.insert(item->id);
                }
            }
            if (replacements.isEmpty())
                return;

            forEachItem(getIndex(&component),
                [this, &replacements](const Item &item) {
#define ADD(COLUMN_TYPE, ITEM_TYPE, PROPERTY)                                 \
    if constexpr (std::is_same_v<decltype(ITEM_TYPE::PROPERTY), ItemId>       \
        && std::string_view(#PROPERTY).ends_with("Id")) {                     \
        if (item.type == Item::Type::ITEM_TYPE) {                             \
            const auto value = static_cast<const ITEM_TYPE &>(item).PROPERTY; \
            if (const auto it = replacements.constFind(value);                \
                it != replacements.cend())                                    \
                setField(&item, COLUMN_TYPE, it.value());                     \
        }                                                                     \
    }
                    ADD_EACH_COLUMN_TYPE()
#undef ADD
                });
        });
    clearUndoStack();
    return referencedItems;
}

const Item *SessionModel::resolveComponentInput(const Item *item) const
{
    if (!item || item->type == Item::Type::Binding || !item->parent
        || item->parent->type != Item::Type::Component
        || item->componentInterface != Item::Interface::Input)
        return item;

    auto visited = QSet<ItemId>{ };
    while (item->parent && item->parent->type == Item::Type::Component
        && item->componentInterface == Item::Interface::Input) {
        if (!item->inputItemId || visited.contains(item->id))
            return nullptr;
        visited.insert(item->id);

        const auto *replacement = findItem(item->inputItemId);
        if (!replacement || replacement->type != item->type)
            return nullptr;
        item = replacement;
    }
    return item;
}

JsonArray SessionModel::getJson(const QModelIndexList &indexes,
    bool serializingScriptItem) const
{
    auto itemArray = JsonArray();
    if (indexes.size() == 1 && !indexes.first().isValid()) {
        for (const Item *item : getItem({ }).items) {
            auto object = JsonObject();
            serialize(object, *item, true, serializingScriptItem);
            itemArray.push_back(object);
        }
    } else {
        for (const QModelIndex &index : indexes) {
            auto object = JsonObject();
            serialize(object, getItem(index), false, serializingScriptItem);
            itemArray.push_back(object);
        }
    }
    return itemArray;
}

void SessionModel::dropJson(const JsonArray &json, int row,
    const QModelIndex &parent)
{
    dropJson(json, row, parent, DropMode::Update);
}

void SessionModel::dropJson(const JsonArray &jsonArray, int row,
    const QModelIndex &parent, DropMode mode)
{
    const auto updateExisting = (mode == DropMode::Update);
    const auto componentContent = (mode == DropMode::ComponentContent
        || mode == DropMode::ComponentImport);
    if (!componentContent && isComponentOrContent(parent)) {
        if (!updateExisting)
            return;
        const auto *parentItem = &getItem(parent);
        for (const auto &value : jsonArray) {
            const auto object = jsonObject(value);
            if (!object || object->count("items"))
                return;
            const auto item = findItem(jsonValue<ItemId>(*object, "id"));
            if (!item || item->parent != parentItem)
                return;
        }
    }

    for (const JsonValue &value : jsonArray)
        if (const auto object = jsonObject(value))
            deserialize(*object, parent, row++, mode);

    // fixup item references
    const auto keys = mDroppedIdsReplaced.keys();
    for (ItemId prevId : keys)
        for (const QModelIndex &reference : std::as_const(mDroppedReferences))
            if (data(reference).toInt() == prevId)
                setData(reference, mDroppedIdsReplaced[prevId]);
    mDroppedIdsReplaced.clear();
    mDroppedReferences.clear();
}

void SessionModel::deserialize(const JsonObject &object,
    const QModelIndex &parent, int row, DropMode mode)
{
    const auto updateExisting = (mode == DropMode::Update);
    const auto forceNewIds = (mode == DropMode::ComponentImport);
    const auto componentContent = (mode == DropMode::ComponentContent
        || mode == DropMode::ComponentImport);
    const auto sourceId =
        (object.count("id") ? jsonValue<int>(object, "id") : 0);
    auto id = (sourceId ? sourceId : getNextItemId());

    if (forceNewIds) {
        id = getNextItemId();
        if (sourceId)
            mDroppedIdsReplaced.insert(sourceId, id);
    }

    auto existingItem = findItem(id);
    auto untitledFileName = QString();
    if (!forceNewIds && existingItem && !updateExisting) {
        // generate new id when it collides with an existing item
        const auto prevId = std::exchange(id, getNextItemId());
        mDroppedIdsReplaced.insert(prevId, id);
        existingItem = nullptr;

        if (mDraggedUntitledFileNames.contains(prevId))
            untitledFileName = mDraggedUntitledFileNames[prevId];
    } else if (mDraggedUntitledFileNames.contains(id)) {
        untitledFileName = mDraggedUntitledFileNames[id];
    }

    if (!componentContent && isComponentOrContent(parent)
        && (!existingItem || existingItem->parent != &getItem(parent)))
        return;

    auto ok = true;
    const auto type = (existingItem ? existingItem->type
            : object.count("type")
            ? getItemTypeByName(jsonValue<QString>(object, "type"), &ok)
            : getDefaultChildType(parent));
    if (!ok)
        return;

    const auto isDynamicGroup =
        (type == Item::Type::Group && jsonValue<bool>(object, "dynamic"));

    const auto index = (existingItem
            ? getIndex(existingItem)
            : insertItem(type, parent, row, id, isDynamicGroup));

    // preserve untitled filenames when dragging/copying
    if (!untitledFileName.isEmpty())
        setField(index, FileName, untitledFileName);

    const auto dropColumn =
        [&](const QString &property, const QModelIndex &index,
            SessionModel::ColumnType column, const QVariant &value) {
            if (property.endsWith("Id")) {
                // reference to an already patched id?
                auto it = mDroppedIdsReplaced.find(value.toInt());
                if (it != mDroppedIdsReplaced.end()) {
                    setField(index, column, it.value());
                    return;
                }
                // otherwise remember reference for fixup
                mDroppedReferences.append(getIndex(index, column));
            }
            setField(index, column, value);
        };

    for (const auto &[key, json] : object) {
        const auto property =
            QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size()));
        auto value = variantFromJson(json);

        if (property == "name") {
            setField(index, Name, value);
        } else if (property == "custom") {
            setField(index, Custom, value);
        } else if (property == "sessionInterface") {
            setField(index, SessionInterface, value);
        } else if (property == "componentInterface") {
            setField(index, ComponentInterface, value);
        } else if (property == "inputItemId") {
            dropColumn(property, index, InputItemId, value);
        } else if (property == "fileName") {
            setField(index, FileName,
                toNativeCanonicalAbsoluteFilePath(value.toString()));
        } else if (property == "target") {
            // TODO: remove, added for backward compatibility
            if (value == "Target2DMultisample")
                value = "Target2D";
            if (value == "Target2DMultisampleArray")
                value = "Target2DArray";
            dropColumn(property, index, TextureTarget, value);
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
                dropColumn(property, index, it->second, value);
        }
    }

    if (!existingItem && type == Item::Type::Component
        && mode != DropMode::ComponentImport)
        mode = DropMode::ComponentContent;
    if (const auto items = jsonArray(object, "items"))
        for (const JsonValue &value : *items)
            if (const auto child = jsonObject(value))
                deserialize(*child, index, -1, mode);
}

void SessionModel::serialize(JsonObject &object, const Item &item,
    bool relativeFilePaths, bool serializingScriptItem) const
{
    object["type"] = getItemTypeName(item.type);
    object["id"] = item.id;
    object["name"] = item.name;
    if (serializingScriptItem || !item.custom.isEmpty())
        object["custom"] = toJsonValue(item.custom);
    if (serializingScriptItem
        || item.sessionInterface != Item::Interface::Internal)
        object["sessionInterface"] = toJsonValue(item.sessionInterface);
    if (serializingScriptItem
        || item.componentInterface != Item::Interface::Internal)
        object["componentInterface"] = toJsonValue(item.componentInterface);
    if (serializingScriptItem || item.inputItemId)
        object["inputItemId"] = item.inputItemId;

    if (auto fileItem = castItem<FileItem>(item)) {
        const auto &fileName = fileItem->fileName;
        if (serializingScriptItem || !fileName.isEmpty()) {
            if (FileDialog::isUntitled(fileName) && !serializingScriptItem) {
                mDraggedUntitledFileNames[item.id] = fileName;
            } else {
                object["fileName"] = (relativeFilePaths
                        ? toForwardSlashRelativeFilePath(fileName)
                        : fileName);
                if (!serializingScriptItem
                    && QFileInfo(fileName).fileName() == item.name)
                    object.erase("name");
            }
        }
    }

#define ADD(COLUMN_TYPE, ITEM_TYPE, PROPERTY)             \
    if (item.type == Item::Type::ITEM_TYPE                \
        && (serializingScriptItem                         \
            || shouldSerializeColumn(item, COLUMN_TYPE))) \
        object[#PROPERTY] =                               \
            toJsonValue(static_cast<const ITEM_TYPE &>(item).PROPERTY);

    ADD_EACH_COLUMN_TYPE()
#undef ADD

    if (!serializingScriptItem && !item.items.empty()
        && !isDynamicGroup(item)) {
        auto items = JsonArray();
        for (const Item *item : item.items) {
            auto sub = JsonObject();
            serialize(sub, *item, relativeFilePaths);
            items.push_back(sub);
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
        for (auto c : {
                 SessionApiVersion,
                 SessionFlipViewport,
                 SessionReverseCulling,
             })
            result &= (column != c || rendererHasSetting(session.renderer, c));
        break;
    }

    case Item::Type::Group: {
        const auto &group = static_cast<const Group &>(item);
        result &= (column != GroupDynamic || (group.dynamic));
        break;
    }

    case Item::Type::Shader: {
        const auto &shader = static_cast<const Shader &>(item);
        result &= (column != ShaderEntryPoint
            || (getShaderLanguage(shader) != Session::ShaderLanguage::GLSL));
        result &= (column != ShaderPreamble || !shader.preamble.isEmpty());
        result &=
            (column != ShaderIncludePaths || !shader.includePaths.isEmpty());
        break;
    }

    case Item::Type::Record: {
        const auto &record = static_cast<const Record &>(item);
        const auto hit = (record.recordType == Record::RecordType::Hit);
        result &= (column != RecordShaderId || !hit);
        result &= (column != RecordIntersectionShaderId
            || (hit && record.intersectionShaderId));
        result &=
            (column != RecordAnyHitShaderId || (hit && record.anyHitShaderId));
        result &= (column != RecordClosestHitShaderId
            || (hit && record.closestHitShaderId));
        break;
    }

    case Item::Type::Texture: {
        const auto &texture = static_cast<const Texture &>(item);
        auto kind = getKind(texture);
        const auto audioSource = isAudioSource(texture.sourceType);
        result &= (column != TextureSourceType
            || texture.sourceType != Texture::SourceType::NoSource);
        result &= (column != TextureAudioVolume || audioSource);
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
        const auto accelerationStructure = (binding.bindingType
            == Binding::BindingType::AccelerationStructure);
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
        result &=
            (column != BindingAccelerationStructureId || accelerationStructure);
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
        result &= (column != CallProgramId || kind.draw || kind.compute);
        result &= (column != CallShaderTableId || kind.trace);
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
            || ((kind.compute || kind.mesh || kind.trace) && !kind.indirect
                && !kind.sound));
        result &= (column != CallWorkGroupsY
            || ((kind.compute || kind.mesh || kind.trace) && !kind.indirect
                && !kind.sound));
        result &= (column != CallWorkGroupsZ
            || ((kind.compute || kind.mesh || kind.trace) && !kind.indirect
                && !kind.sound));
        result &= (column != CallExecuteOn || !kind.sound);
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
        const auto hasTriangles =
            (geometry.geometryType == Geometry::GeometryType::Triangles);
        result &= (column != GeometryIndexBufferBlockId || hasTriangles);
        result &= (column != GeometryTransformBufferBlockId || hasTriangles);
        break;
    }

    default: break;
    }
    return result;
}

bool rendererHasSetting(Session::Renderer renderer,
    SessionModel::ColumnType column)
{
    using R = Session::Renderer;
    using CT = SessionModel::ColumnType;
    switch (column) {
    default:                        break;
    case CT::SessionApiVersion:     return (renderer == R::Vulkan);
    case CT::SessionReverseCulling: return (renderer != R::OpenGL);
    case CT::SessionFlipViewport:   return (renderer != R::OpenGL);
    }
    return false;
}
