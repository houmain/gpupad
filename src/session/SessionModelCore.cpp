#include "SessionModelCore.h"
#include "FileDialog.h"
#include "SessionModelPriv.h"

namespace {
    template <typename Redo, typename Undo, typename Free>
    class UndoCommand : public QUndoCommand
    {
    public:
        UndoCommand(QString text, Redo &&redo, Undo &&undo, Free &&free,
            bool owns)
            : QUndoCommand(text)
            , mRedo(std::forward<Redo>(redo))
            , mUndo(std::forward<Undo>(undo))
            , mFree(std::forward<Free>(free))
            , mOwns(owns)
        {
        }
        ~UndoCommand() override
        {
            if (mOwns)
                mFree();
        }
        void redo() override
        {
            mRedo();
            mOwns = !mOwns;
        }
        void undo() override
        {
            mUndo();
            mOwns = !mOwns;
        }

    private:
        const Redo mRedo;
        const Undo mUndo;
        const Free mFree;
        bool mOwns;
    };

    template <typename Redo, typename Undo, typename Free>
    auto makeUndoCommand(QString text, Redo &&redo, Undo &&undo, Free &&free,
        bool owns = false)
    {
        return new UndoCommand<Redo, Undo, Free>(text, std::forward<Redo>(redo),
            std::forward<Undo>(undo), std::forward<Free>(free), owns);
    }

    class MergingUndoCommand final : public QUndoCommand
    {
    public:
        MergingUndoCommand(int id, QUndoCommand *firstCommand)
            : QUndoCommand(firstCommand->text())
            , mId(id)
            , mCommands{ firstCommand }
        {
        }
        ~MergingUndoCommand() override { qDeleteAll(mCommands); }
        void redo() override
        {
            std::for_each(mCommands.begin(), mCommands.end(),
                [](auto c) { c->redo(); });
        }
        void undo() override
        {
            std::for_each(mCommands.rbegin(), mCommands.rend(),
                [](auto c) { c->undo(); });
        }
        int id() const override { return mId; }
        bool mergeWith(const QUndoCommand *other) override
        {
            Q_ASSERT(other->id() == id());
            auto &c = static_cast<const MergingUndoCommand *>(other)->mCommands;
            mCommands.append(c);
            c.clear();
            return true;
        }

    private:
        const int mId;
        mutable QList<QUndoCommand *> mCommands;
    };

    Item *allocateItem(Item::Type type)
    {
        switch (type) {
        case Item::Type::Root:       break;
        case Item::Type::Session:    return new Session();
        case Item::Type::Group:      return new Group();
        case Item::Type::Buffer:     return new Buffer();
        case Item::Type::Block:      return new Block();
        case Item::Type::Field:      return new Field();
        case Item::Type::Texture:    return new Texture();
        case Item::Type::Program:    return new Program();
        case Item::Type::Shader:     return new Shader();
        case Item::Type::Binding:    return new Binding();
        case Item::Type::Stream:     return new Stream();
        case Item::Type::Attribute:  return new Attribute();
        case Item::Type::Target:     return new Target();
        case Item::Type::Attachment: return new Attachment();
        case Item::Type::Call:       return new Call();
        case Item::Type::Script:     return new Script();
        }
        Q_UNREACHABLE();
        return nullptr;
    }

    Item *cloneItem(const Item &item)
    {
        const auto copy = [&]() -> Item * {
            switch (item.type) {
            case Item::Type::Root: break;
            case Item::Type::Session:
                return new Session(static_cast<const Session &>(item));
            case Item::Type::Group:
                return new Group(static_cast<const Group &>(item));
            case Item::Type::Buffer:
                return new Buffer(static_cast<const Buffer &>(item));
            case Item::Type::Block:
                return new Block(static_cast<const Block &>(item));
            case Item::Type::Field:
                return new Field(static_cast<const Field &>(item));
            case Item::Type::Texture:
                return new Texture(static_cast<const Texture &>(item));
            case Item::Type::Program:
                return new Program(static_cast<const Program &>(item));
            case Item::Type::Shader:
                return new Shader(static_cast<const Shader &>(item));
            case Item::Type::Binding:
                return new Binding(static_cast<const Binding &>(item));
            case Item::Type::Stream:
                return new Stream(static_cast<const Stream &>(item));
            case Item::Type::Target:
                return new Target(static_cast<const Target &>(item));
            case Item::Type::Attribute:
                return new Attribute(static_cast<const Attribute &>(item));
            case Item::Type::Attachment:
                return new Attachment(static_cast<const Attachment &>(item));
            case Item::Type::Call:
                return new Call(static_cast<const Call &>(item));
            case Item::Type::Script:
                return new Script(static_cast<const Script &>(item));
            }
            Q_UNREACHABLE();
            return nullptr;
        }();

        for (auto &child : copy->items) {
            child = cloneItem(*child);
            child->parent = copy;
        }
        return copy;
    }
} // namespace

SessionModelCore::SessionModelCore(QObject *parent) : QAbstractItemModel(parent)
{
    clear();
}

SessionModelCore &SessionModelCore::operator=(const SessionModelCore &rhs)
{
    if (this != &rhs) {
        clear();

        // replace session item
        Q_ASSERT(mRoot.items.size() == 1);
        Q_ASSERT(rhs.mRoot.items.size() == 1);
        delete mRoot.items[0];
        mRoot.items[0] = cloneItem(*rhs.mRoot.items[0]);
        mRoot.items[0]->parent = &mRoot;

        forEachItem(mRoot,
            [&](const Item &item) { mItemsById.insert(item.id, &item); });

        mNextItemId = rhs.mNextItemId;
    }
    return *this;
}

SessionModelCore::~SessionModelCore()
{
    Q_ASSERT(rowCount() == 1);
    Q_ASSERT(undoStack().isClean());
}

void SessionModelCore::clear()
{
    undoStack().clear();
    undoStack().setUndoLimit(1);

    deleteItem(QModelIndex());
    insertItem(Item::Type::Session, QModelIndex());

    undoStack().clear();
    undoStack().setUndoLimit(0);
}

QString SessionModelCore::getTypeName(Item::Type type) const
{
    return QMetaEnum::fromType<Item::Type>().valueToKey(static_cast<int>(type));
}

Item::Type SessionModelCore::getTypeByName(const QString &name, bool &ok) const
{
    auto type =
        QMetaEnum::fromType<Item::Type>().keyToValue(qPrintable(name), &ok);
    return (ok ? static_cast<Item::Type>(type) : Item::Type::Group);
}

bool SessionModelCore::canContainType(const QModelIndex &index,
    Item::Type type) const
{
    switch (getItemType(index)) {
    case Item::Type::Root: return (type == Item::Type::Session);
    case Item::Type::Session:
    case Item::Type::Group:
        switch (type) {
        case Item::Type::Group:
        case Item::Type::Buffer:
        case Item::Type::Texture:
        case Item::Type::Program:
        case Item::Type::Binding:
        case Item::Type::Stream:
        case Item::Type::Target:
        case Item::Type::Call:
        case Item::Type::Script:  return true;
        default:                  return false;
        }
    case Item::Type::Buffer:  return (type == Item::Type::Block);
    case Item::Type::Block:   return (type == Item::Type::Field);
    case Item::Type::Program: return (type == Item::Type::Shader);
    case Item::Type::Stream:  return (type == Item::Type::Attribute);
    case Item::Type::Target:  return (type == Item::Type::Attachment);
    default:                  return false;
    }
}

Item::Type SessionModelCore::getDefaultChildType(const QModelIndex &index) const
{
    switch (getItemType(index)) {
    case Item::Type::Buffer:  return Item::Type::Block;
    case Item::Type::Block:   return Item::Type::Field;
    case Item::Type::Program: return Item::Type::Shader;
    case Item::Type::Stream:  return Item::Type::Attribute;
    case Item::Type::Target:  return Item::Type::Attachment;
    default:                  return Item::Type::Group;
    }
}

QModelIndex SessionModelCore::index(int row, int column,
    const QModelIndex &parent) const
{
    const auto &parentItem = getItem(parent);
    if (row >= 0 && row < parentItem.items.size())
        return getIndex(parentItem.items.at(row),
            static_cast<ColumnType>(column));

    return {};
}

QModelIndex SessionModelCore::parent(const QModelIndex &child) const
{
    if (child.isValid())
        return getIndex(getItem(child).parent, ColumnType::Name);
    return {};
}

int SessionModelCore::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return getItem(parent).items.size();
    return mRoot.items.size();
}

int SessionModelCore::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant SessionModelCore::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole && role != Qt::EditRole
        && role != Qt::CheckStateRole)
        return {};

    const auto &item = getItem(index);

    if (role == Qt::CheckStateRole) {
        if (auto call = castItem<Call>(item))
            return (call->checked ? Qt::Checked : Qt::Unchecked);
        return {};
    }

    const auto column = static_cast<ColumnType>(index.column());

    switch (column) {
    case ColumnType::Name: return item.name;

    case ColumnType::None: return {};

    case ColumnType::FileName:
        if (auto fileItem = castItem<FileItem>(item))
            return fileItem->fileName;
        break;

#define ADD(COLUMN_TYPE, ITEM_TYPE, PROPERTY)                     \
    case ColumnType::COLUMN_TYPE:                                 \
        if (item.type == Item::Type::ITEM_TYPE)                   \
            return static_cast<const ITEM_TYPE &>(item).PROPERTY; \
        break;

        ADD_EACH_COLUMN_TYPE()
#undef ADD
    }
    return {};
}

bool SessionModelCore::setData(const QModelIndex &index, const QVariant &value,
    int role)
{
    if (role != Qt::EditRole && role != Qt::CheckStateRole)
        return false;

    auto &item = getItemRef(index);

    if (role == Qt::CheckStateRole) {
        auto checked = (value == Qt::Checked);
        switch (item.type) {
        case Item::Type::Call:
            undoableAssignment(index, &static_cast<Call &>(item).checked,
                checked);
            return true;
        default: return false;
        }
    }

    switch (static_cast<ColumnType>(index.column())) {
    case ColumnType::Name:
        if (value.toString().isEmpty())
            return false;
        undoableAssignment(index, &item.name, value.toString());
        return true;

    case ColumnType::None: return true;

    case ColumnType::FileName:
        if (castItem<FileItem>(item)) {
            undoableFileNameAssignment(index, static_cast<FileItem &>(item),
                value.toString());
            return true;
        }
        break;

#define ADD(COLUMN_TYPE, ITEM_TYPE, PROPERTY)                          \
    case ColumnType::COLUMN_TYPE:                                      \
        if (item.type == Item::Type::ITEM_TYPE) {                      \
            auto &property = static_cast<ITEM_TYPE &>(item).PROPERTY;  \
            undoableAssignment(index, &property,                       \
                fromVariant<std::decay_t<decltype(property)>>(value)); \
            return true;                                               \
        }                                                              \
        break;

        ADD_EACH_COLUMN_TYPE()
#undef ADD
    }
    return false;
}

void SessionModelCore::insertItem(Item *item, const QModelIndex &parent,
    int row)
{
    Q_ASSERT(item && !item->parent);
    auto &parentItem = getItemRef(parent);
    if (row < 0 || row > parentItem.items.size())
        row = parentItem.items.size();

    item->parent = &parentItem;

    if (!item->id)
        item->id = getNextItemId();
    mNextItemId = std::max(mNextItemId, item->id + 1);

    undoableInsertItem(&parentItem.items, item, parent, row);
}

ItemId SessionModelCore::getNextItemId()
{
    while (findItem(mNextItemId))
        mNextItemId++;
    return mNextItemId;
}

QModelIndex SessionModelCore::sessionItemIndex() const
{
    return index(0, 0);
}

const Session &SessionModelCore::sessionItem() const
{
    auto session = item<Session>(sessionItemIndex());
    Q_ASSERT(session);
    if (session)
        return *session;
    static const Session sEmpty{};
    return sEmpty;
}

QModelIndex SessionModelCore::findChildByName(const QModelIndex &parent,
    const QString &name) const
{
    for (auto i = 0; i < rowCount(parent); ++i) {
        auto child = index(i, 0, parent);
        if (getItem(child).name == name)
            return child;
    }
    return {};
}

QModelIndex SessionModelCore::insertItem(Item::Type type, QModelIndex parent,
    int row, ItemId id)
{
    // insert as sibling, when parent cannot contain an item of type
    while (!canContainType(parent, type)) {
        if (!parent.isValid())
            return {};

        // insert new item before sinks and after sources
        const auto insertBefore = [&]() {
            const auto parentType = getItemType(parent);
            if (parentType != type)
                switch (parentType) {
                case Item::Type::Buffer:
                case Item::Type::Texture:
                case Item::Type::Script:  return true;
                default:                  break;
                }
            return false;
        };
        row = parent.row() + (insertBefore() ? 0 : 1);
        parent = parent.parent();
    }

    const auto typeName = getTypeName(type);
    auto name = typeName;
    for (auto i = 2; findChildByName(parent, name).isValid(); ++i)
        name = typeName + QStringLiteral(" %1").arg(i);

    auto item = allocateItem(type);
    item->type = type;
    item->name = name;
    item->id = id;
    insertItem(item, parent, row);
    return getIndex(item, ColumnType::Name);
}

void SessionModelCore::deleteItem(const QModelIndex &index)
{
    for (auto i = rowCount(index) - 1; i >= 0; --i)
        deleteItem(this->index(i, 0, index));

    if (index.isValid()) {
        auto &item = getItemRef(index);
        undoableRemoveItem(&item.parent->items, &item, index);
    }
}

const Item *SessionModelCore::findItem(ItemId id) const
{
    return mItemsById.value(id);
}

QModelIndex SessionModelCore::getIndex(const Item *item,
    ColumnType column) const
{
    if (!item || item == &mRoot)
        return {};

    auto itemPtr = const_cast<Item *>(item);
    return createIndex(item->parent->items.indexOf(itemPtr), column, itemPtr);
}

QModelIndex SessionModelCore::getIndex(const QModelIndex &rowIndex,
    ColumnType column) const
{
    return index(rowIndex.row(), column, rowIndex.parent());
}

const Item &SessionModelCore::getItem(const QModelIndex &index) const
{
    if (index.isValid())
        return *static_cast<const Item *>(index.internalPointer());
    return mRoot;
}

Item &SessionModelCore::getItemRef(const QModelIndex &index)
{
    if (index.isValid())
        return *static_cast<Item *>(index.internalPointer());
    return mRoot;
}

ItemId SessionModelCore::getItemId(const QModelIndex &index) const
{
    return getItem(index).id;
}

Item::Type SessionModelCore::getItemType(const QModelIndex &index) const
{
    return getItem(index).type;
}

void SessionModelCore::insertItem(QList<Item *> *list, Item *item,
    const QModelIndex &parent, int row)
{
    mItemsById[item->id] = item;
    beginInsertRows(parent, row, row);
    list->insert(row, item);
    endInsertRows();
}

void SessionModelCore::removeItem(QList<Item *> *list,
    const QModelIndex &parent, int row)
{
    mItemsById.remove(list->at(row)->id);
    beginRemoveRows(parent, row, row);
    list->removeAt(row);
    endRemoveRows();
}

bool SessionModelCore::removeRows(int row, int count, const QModelIndex &parent)
{
    mUndoStack.beginMacro("Remove");

    for (auto i = count - 1; i >= 0; --i)
        deleteItem(index(row + i, 0, parent));

    mUndoStack.endMacro();
    return true;
}

void SessionModelCore::pushUndoCommand(QUndoCommand *command)
{
    mUndoStack.push(command);
}

void SessionModelCore::undoableInsertItem(QList<Item *> *list, Item *item,
    const QModelIndex &parent, int row)
{
    pushUndoCommand(makeUndoCommand(
        "Insert", [=, this]() { insertItem(list, item, parent, row); },
        [=, this]() { removeItem(list, parent, row); }, [=]() { delete item; },
        true));
}

void SessionModelCore::undoableRemoveItem(QList<Item *> *list, Item *item,
    const QModelIndex &index)
{
    pushUndoCommand(makeUndoCommand(
        "Remove",
        [=, this]() { removeItem(list, index.parent(), index.row()); },
        [=, this]() { insertItem(list, item, index.parent(), index.row()); },
        [=]() { delete item; }, false));
}

template <typename T, typename S>
void SessionModelCore::assignment(const QModelIndex &index, T *to, S &&value)
{
    Q_ASSERT(index.isValid());
    if (*to != value) {
        *to = value;
        Q_EMIT dataChanged(index, index);
    }
}

template <typename T, typename S>
void SessionModelCore::undoableAssignment(const QModelIndex &index, T *to,
    S &&value, int mergeId)
{
    Q_ASSERT(index.isValid());
    if (*to != value) {
        if (mergeId < 0)
            mergeId = -index.column();
        mergeId += reinterpret_cast<uintptr_t>(index.internalPointer());

        pushUndoCommand(new MergingUndoCommand(mergeId,
            makeUndoCommand(
                "Edit ",
                [=, this, value = std::forward<S>(value)]() {
                    *to = static_cast<T>(value);
                    Q_EMIT dataChanged(index, index);
                },
                [=, this, orig = *to]() {
                    *to = orig;
                    Q_EMIT dataChanged(index, index);
                },
                []() {})));
    }
}

void SessionModelCore::undoableFileNameAssignment(const QModelIndex &index,
    FileItem &item, QString fileName)
{
    // do not add undo command, when untitled replaces empty
    if (FileDialog::isEmptyOrUntitled(item.fileName)
        && FileDialog::isEmptyOrUntitled(fileName)) {

        if (item.fileName != fileName) {
            item.fileName = fileName;
            Q_EMIT dataChanged(index, index);
        }
        return;
    }

    if (item.fileName != fileName)
        undoableAssignment(index, &item.fileName, fileName);
}
