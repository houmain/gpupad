#include "SessionModelCore.h"
#include "SessionModelPriv.h"
#include "FileDialog.h"

namespace {
    template<typename Redo, typename Undo, typename Free>
    class UndoCommand : public QUndoCommand
    {
    public:
        UndoCommand(QString text,
            Redo &&redo, Undo &&undo, Free &&free, bool owns)
            : QUndoCommand(text)
            , mRedo(std::forward<Redo>(redo))
            , mUndo(std::forward<Undo>(undo))
            , mFree(std::forward<Free>(free))
            , mOwns(owns) { }
        ~UndoCommand() override { if (mOwns) mFree(); }
        void redo() override { mRedo(); mOwns = !mOwns; }
        void undo() override { mUndo(); mOwns = !mOwns; }

    private:
        const Redo mRedo;
        const Undo mUndo;
        const Free mFree;
        bool mOwns;
    };

    template<typename Redo, typename Undo, typename Free>
    auto makeUndoCommand(QString text, Redo &&redo, Undo &&undo, Free &&free,
        bool owns = false)
    {
        return new UndoCommand<Redo, Undo, Free>(text,
            std::forward<Redo>(redo),
            std::forward<Undo>(undo),
            std::forward<Free>(free),
            owns);
    }

    class MergingUndoCommand : public QUndoCommand
    {
    public:
        MergingUndoCommand(int id, QUndoCommand *firstCommand)
            : QUndoCommand(firstCommand->text())
            , mId(id)
            , mCommands{ firstCommand } { }
        ~MergingUndoCommand() override {
            qDeleteAll(mCommands);
        }
        void redo() override {
            std::for_each(mCommands.begin(), mCommands.end(),
                [](auto c) { c->redo(); });
        }
        void undo() override {
            std::for_each(mCommands.rbegin(), mCommands.rend(),
                [](auto c) { c->undo(); });
        }
        int id() const override { return mId; }
        bool mergeWith(const QUndoCommand* other) override {
            Q_ASSERT(other->id() == id());
            auto &c = static_cast<const MergingUndoCommand*>(other)->mCommands;
            mCommands.append(c);
            c.clear();
            return true;
        }

    private:
        const int mId;
        mutable QList<QUndoCommand*> mCommands;
    };
} // namespace

SessionModelCore::SessionModelCore(QObject *parent)
    : QAbstractItemModel(parent)
    , mRoot(new Group())
{
}

SessionModelCore::~SessionModelCore() = default;

QString SessionModelCore::getTypeName(Item::Type type) const
{
    return QMetaEnum::fromType<Item::Type>().valueToKey(static_cast<int>(type));
}

Item::Type SessionModelCore::getTypeByName(const QString &name) const
{
    auto ok = false;
    auto type = QMetaEnum::fromType<Item::Type>().keyToValue(qPrintable(name), &ok);
    return (ok ? static_cast<Item::Type>(type) : Item::Type::Group);
}

bool SessionModelCore::canContainType(const QModelIndex &index, Item::Type type) const
{
    switch (getItemType(index)) {
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
                case Item::Type::Script:
                    return true;
                default:
                    return false;
            }
        case Item::Type::Buffer: return (type == Item::Type::Column);
        case Item::Type::Texture: return (type == Item::Type::Image);
        case Item::Type::Program: return (type == Item::Type::Shader);
        case Item::Type::Stream: return (type == Item::Type::Attribute);
        case Item::Type::Target: return (type == Item::Type::Attachment);
        default:
            return false;
    }
}

QModelIndex SessionModelCore::index(int row, int column,
    const QModelIndex &parent) const
{
    const auto &parentItem = getItem(parent);
    if (row >= 0 && row < parentItem.items.size())
        return getIndex(parentItem.items.at(row), column);

    return { };
}

QModelIndex SessionModelCore::parent(const QModelIndex &child) const
{
    if (child.isValid())
        return getIndex(getItem(child).parent, 0);
    return { };
}

int SessionModelCore::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return getItem(parent).items.size();
    return mRoot->items.size();
}

int SessionModelCore::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant SessionModelCore::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole &&
        role != Qt::EditRole &&
        role != Qt::CheckStateRole)
        return { };

    const auto &item = getItem(index);

    if (role == Qt::CheckStateRole) {
        if (auto call = castItem<Call>(item))
            return (call->checked ? Qt::Checked : Qt::Unchecked);
        return { };
    }

    const auto column = static_cast<ColumnType>(index.column());

    switch (column) {
        case ColumnType::Name:
            return item.name;

        case ColumnType::FileName:
            if (auto fileItem = castItem<FileItem>(item))
                return fileItem->fileName;
            break;

#define ADD(COLUMN_TYPE, ITEM_TYPE, PROPERTY) \
        case ColumnType::COLUMN_TYPE: \
            if (item.type == Item::Type::ITEM_TYPE) \
                return static_cast<const ITEM_TYPE&>(item).PROPERTY; \
            break;

        ADD_EACH_COLUMN_TYPE()
        ADD(BindingValueCount, Binding, valueCount)
        ADD(BindingCurrentValue, Binding, currentValue)
#undef ADD

#define ADD(COLUMN_TYPE, PROPERTY) \
        case ColumnType::COLUMN_TYPE: \
            if (item.type == Item::Type::Binding) { \
                const auto &binding = static_cast<const Binding&>(item); \
                const auto current = static_cast<size_t>(binding.currentValue); \
                return binding.values[current].PROPERTY; \
            } \
            break;

        ADD_EACH_BINDING_VALUE_COLUMN_TYPE()
#undef ADD

        case FirstBindingValue:
        case LastBindingValue:
            break;
    }
    return { };
}

bool SessionModelCore::setData(const QModelIndex &index,
    const QVariant &value, int role)
{
    if (role != Qt::EditRole &&
        role != Qt::CheckStateRole)
        return false;

    auto &item = getItemRef(index);

    if (role == Qt::CheckStateRole) {
        auto checked = (value == Qt::Checked);
        switch (item.type) {
            case Item::Type::Call:
                undoableAssignment(index,
                    &static_cast<Call&>(item).checked, checked);
                return true;
            default:
                return false;
        }
    }

    switch (static_cast<ColumnType>(index.column())) {
        case ColumnType::Name:
            if (value.toString().isEmpty())
                return false;
            undoableAssignment(index, &item.name, value.toString());
            return true;

        case ColumnType::FileName:
            if (castItem<FileItem>(item)) {
                undoableFileNameAssignment(index,
                    static_cast<FileItem&>(item), value.toString());
                return true;
            }
            break;

        case ColumnType::BindingCurrentValue:
            if (item.type == Item::Type::Binding) {
                auto &binding = static_cast<Binding&>(item);
                auto newValue = qBound(0, value.toInt(),
                    static_cast<int>(binding.values.size() - 1));
                if (binding.currentValue != newValue) {
                    binding.currentValue = newValue;
                    emit dataChanged(
                        getIndex(&item, ColumnType::FirstBindingValue),
                        getIndex(&item, ColumnType::LastBindingValue));
                }
                return true;
            }
            break;

#define ADD(COLUMN_TYPE, ITEM_TYPE, PROPERTY) \
        case ColumnType::COLUMN_TYPE: \
            if (item.type == Item::Type::ITEM_TYPE) { \
                auto &property = static_cast<ITEM_TYPE&>(item).PROPERTY; \
                undoableAssignment(index, &property, \
                    fromVariant<std::decay_t<decltype(property)>>(value)); \
                return true; \
            } \
            break;

        ADD_EACH_COLUMN_TYPE()
        ADD(BindingValueCount, Binding, valueCount)
#undef ADD

#define ADD(COLUMN_TYPE, PROPERTY) \
        case ColumnType::COLUMN_TYPE: \
            if (item.type == Item::Type::Binding) { \
                auto &binding = static_cast<Binding&>(item); \
                const auto current = static_cast<size_t>(binding.currentValue); \
                auto &property = binding.values[current].PROPERTY; \
                undoableAssignment(index, &property, \
                    fromVariant<std::decay_t<decltype(property)>>(value)); \
                return true; \
            } \
            break;

        ADD_EACH_BINDING_VALUE_COLUMN_TYPE()
#undef ADD

        case FirstBindingValue:
        case LastBindingValue:
            break;
    }
    return false;
}

void SessionModelCore::insertItem(Item *item, const QModelIndex &parent, int row)
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
    return mNextItemId++;
}

QModelIndex SessionModelCore::insertItem(Item::Type type, QModelIndex parent,
    int row, ItemId id)
{
    // insert as sibling, when parent cannot contain an item of type
    while (!canContainType(parent, type)) {
        if (!parent.isValid())
            return { };
        row = parent.row() + 1;
        parent = parent.parent();
    }

    auto insert = [&](auto item) {
        item->type = type;
        item->name = getTypeName(type);
        item->id = id;
        insertItem(item, parent, row);
        return getIndex(item);
    };

    switch (type) {
        case Item::Type::Group: return insert(new Group());
        case Item::Type::Buffer: return insert(new Buffer());
        case Item::Type::Column: return insert(new Column());
        case Item::Type::Texture: return insert(new Texture());
        case Item::Type::Image: return insert(new Image());
        case Item::Type::Program: return insert(new Program());
        case Item::Type::Shader: return insert(new Shader());
        case Item::Type::Binding: return insert(new Binding());
        case Item::Type::Stream: return insert(new Stream());
        case Item::Type::Attribute: return insert(new Attribute());
        case Item::Type::Target: return insert(new Target());
        case Item::Type::Attachment: return insert(new Attachment());
        case Item::Type::Call: return insert(new Call());
        case Item::Type::Script: return insert(new Script());
    }
    return { };
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

const Item* SessionModelCore::findItem(ItemId id) const
{
    // TODO: improve performance
    const Item* result = nullptr;
    forEachItem(*mRoot, [&](const Item &item) {
        if (item.id == id)
            result = &item;
    });
    return result;
}

QModelIndex SessionModelCore::getIndex(const Item *item, int column) const
{
    Q_ASSERT(item);
    if (item == mRoot.data())
        return { };

    auto itemPtr = const_cast<Item*>(item);
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
        return *static_cast<const Item*>(index.internalPointer());
    return *mRoot;
}

Item &SessionModelCore::getItemRef(const QModelIndex &index)
{
    if (index.isValid())
        return *static_cast<Item*>(index.internalPointer());
    return *mRoot;
}

ItemId SessionModelCore::getItemId(const QModelIndex &index) const
{
    return getItem(index).id;
}

Item::Type SessionModelCore::getItemType(const QModelIndex &index) const
{
    return getItem(index).type;
}

void SessionModelCore::insertItem(QList<Item*> *list, Item *item,
        const QModelIndex &parent, int row)
{
    beginInsertRows(parent, row, row);
    list->insert(row, item);
    endInsertRows();
}

void SessionModelCore::removeItem(QList<Item*> *list,
    const QModelIndex &parent, int row)
{
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

void SessionModelCore::undoableInsertItem(QList<Item*> *list, Item *item,
    const QModelIndex &parent, int row)
{
    mUndoStack.push(makeUndoCommand("Insert",
        [=](){ insertItem(list, item, parent, row); },
        [=](){ removeItem(list, parent, row); },
        [=](){ delete item; },
        true));
}

void SessionModelCore::undoableRemoveItem(QList<Item*> *list, Item *item,
    const QModelIndex &index)
{
    mUndoStack.push(makeUndoCommand("Remove",
        [=](){ removeItem(list, index.parent(), index.row()); },
        [=](){ insertItem(list, item, index.parent(), index.row()); },
        [=](){ delete item; },
        false));
}

template<typename T, typename S>
void SessionModelCore::assignment(const QModelIndex &index, T *to, S &&value)
{
    Q_ASSERT(index.isValid());
    if (*to != value) {
        *to = value;
        emit dataChanged(index, index);
    }
}

template<typename T, typename S>
void SessionModelCore::undoableAssignment(const QModelIndex &index, T *to,
    S &&value, int mergeId)
{
    Q_ASSERT(index.isValid());
    if (*to != value) {
        if (mergeId < 0)
            mergeId = -index.column();
        mergeId += reinterpret_cast<uintptr_t>(index.internalPointer());

        mUndoStack.push(new MergingUndoCommand(mergeId,
            makeUndoCommand("Edit ",
                [=, value = std::forward<S>(value)]()
                { *to = static_cast<T>(value); emit dataChanged(index, index); },
                [=, orig = *to]()
                { *to = orig; emit dataChanged(index, index); },
                [](){})));
    }
}

void SessionModelCore::undoableFileNameAssignment(const QModelIndex &index,
    FileItem &item, QString fileName)
{
    if (item.fileName != fileName) {
        mUndoStack.beginMacro("Edit");

        if (item.name == getTypeName(item.type) ||
            item.name == FileDialog::getFileTitle(item.fileName)) {

            auto newFileName =
                (FileDialog::isEmptyOrUntitled(fileName) ?
                    getTypeName(item.type) : fileName);

            setData(getIndex(&item, Name),
                FileDialog::getFileTitle(newFileName));
        }
        undoableAssignment(index, &item.fileName, fileName);
        mUndoStack.endMacro();
    }
}
