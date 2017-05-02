#include "BinaryEditor.h"
#include "BinaryFile.h"
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMenu>
#include <QDoubleSpinBox>
#include <QItemDelegate>

namespace {
    using DataType = BinaryEditor::DataType;

    template <typename T> void setRange(QDoubleSpinBox *spinBox)
    { spinBox->setRange(std::numeric_limits<T>::lowest(),
                        std::numeric_limits<T>::max()); }

    template <typename T>
    QString toHexString(T value, int digits)
    { return QString::number(value, 16).toUpper().rightJustified(digits, '0'); }

    int getTypeSize(DataType type)
    {
        switch (type) {
            case DataType::Int8: return 1;
            case DataType::Int16: return 2;
            case DataType::Int32: return 4;
            case DataType::Int64: return 8;
            case DataType::Uint8: return 1;
            case DataType::Uint16: return 2;
            case DataType::Uint32: return 4;
            case DataType::Uint64: return 8;
            case DataType::Float: return 4;
            case DataType::Double: return 8;
        }
        return 0;
    }

    class SpinBoxDelegate : public QItemDelegate
    {
    public:
        using QItemDelegate::QItemDelegate;

        QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
            const QModelIndex &index) const
        {
            auto *editor = new QDoubleSpinBox(parent);
            editor->setDecimals(0);
            editor->setButtonSymbols(QAbstractSpinBox::NoButtons);
            editor->setFrame(false);

            auto dataType = static_cast<DataType>(index.data(Qt::UserRole).toInt());
            switch (dataType) {
                case DataType::Int8: setRange<int8_t>(editor); break;
                case DataType::Int16: setRange<int16_t>(editor); break;
                case DataType::Int32: setRange<int32_t>(editor); break;
                case DataType::Int64: setRange<int64_t>(editor); break;
                case DataType::Uint8: setRange<uint8_t>(editor); break;
                case DataType::Uint16: setRange<uint16_t>(editor); break;
                case DataType::Uint32: setRange<uint32_t>(editor); break;
                case DataType::Uint64: setRange<uint64_t>(editor); break;
                case DataType::Float:
                    setRange<float>(editor);
                    editor->setDecimals(3);
                    editor->setSingleStep(0.1);
                    break;
                case DataType::Double:
                    setRange<double>(editor);
                    editor->setDecimals(3);
                    editor->setSingleStep(0.1);
                    break;
            }
            return editor;
        }

        void setEditorData(QWidget *editor, const QModelIndex &index) const
        {
            auto value = index.model()->data(index, Qt::EditRole).toDouble();
            auto *spinBox = static_cast<QDoubleSpinBox*>(editor);
            spinBox->setValue(value);
        }

        void setModelData(QWidget *editor, QAbstractItemModel *model,
                                           const QModelIndex &index) const
        {
            auto *spinBox = static_cast<QDoubleSpinBox*>(editor);
            spinBox->interpretText();
            auto value = spinBox->value();
            model->setData(index, value, Qt::EditRole);
        }

        void updateEditorGeometry(QWidget *editor,
            const QStyleOptionViewItem &option, const QModelIndex &) const
        {
            editor->setGeometry(option.rect);
        }
    };

    class HexModel : public QAbstractTableModel
    {
    private:
        int mOffset;
        int mStride;
        int mRowCount;
        const QByteArray &mData;

    public:
        HexModel(BinaryEditor *editor, const QByteArray *data)
          : QAbstractTableModel(editor)
          , mOffset(editor->offset())
          , mStride(editor->stride())
          , mRowCount(editor->rowCount())
          , mData(*data)
        {
            auto rowCount = mData.size() / mStride + 2;
            while (getOffset(rowCount - 1, 0) >= mData.size())
                --rowCount;

            const auto lastRow =
                (mOffset + mStride * mRowCount + mStride - 1) / mStride;

            mRowCount = std::max(rowCount, lastRow);
        }

        int rowCount(const QModelIndex&) const override
        {
            return mRowCount;
        }

        int columnCount(const QModelIndex&) const override
        {
            return mStride;
        }

        int getOffset(int row, int column) const
        {
            return mStride * row + column -
                (mStride - (mOffset % mStride)) % mStride;
        }

        QVariant data(const QModelIndex &index, int role) const override
        {
            if (!index.isValid() ||
                (role != Qt::DisplayRole &&
                 role != Qt::EditRole &&
                 role != Qt::TextAlignmentRole))
            return QVariant();

            if (role == Qt::TextAlignmentRole) {
                if (index.column() == 0)
                    return Qt::AlignRight + Qt::AlignVCenter;
                if (index.row() == (mOffset + mStride - 1) / mStride)
                    return Qt::AlignHCenter + Qt::AlignBottom;
                return Qt::AlignHCenter + Qt::AlignVCenter;
            }

            if (role == Qt::DisplayRole || role == Qt::EditRole) {
                auto offset = getOffset(index.row(), index.column());
                if (offset < 0 || offset >= mData.size())
                    return QVariant();

                return toHexString(
                    static_cast<uint8_t>(mData.data()[offset]), 2);
            }
            return QVariant();
        }

        QVariant headerData(int section,
            Qt::Orientation orientation, int role) const override
        {
            if (role == Qt::DisplayRole) {
                if (orientation == Qt::Horizontal)
                    return toHexString(section, 2);

                auto offset = getOffset(section, 0);
                if (offset >= 0)
                    return toHexString(offset, 4);

            }
            return { };
        }

        Qt::ItemFlags flags(const QModelIndex &) const override
        {
            return { };
        }
    };

    class DataModel : public QAbstractTableModel
    {
    private:
        struct Column
        {
            int index;
            int offset;
            DataType type;
            int arity;
            QString name;
            bool editable;
        };

        const Column *getColumn(int index) const
        {
            for (const auto &c : mColumns)
                if (index < c.index + c.arity)
                    return &c;
            return nullptr;
        }

        int getColumnOffset(int index) const
        {
            if (auto column = getColumn(index))
                return column->offset +
                    getTypeSize(column->type) * (index - column->index);
            return 0;
        }

        int mOffset;
        int mStride;
        int mRowCount;
        BinaryFile &mFile;
        QList<Column> mColumns;
        int mColumnCount{ };

    public:
        DataModel(BinaryEditor *editor, BinaryFile *file)
          : QAbstractTableModel(editor)
          , mOffset(editor->offset())
          , mStride(editor->stride())
          , mRowCount(editor->rowCount())
          , mFile(*file)
        {
            auto index = 0;
            auto offset = 0;
            for (auto i = 0; i < editor->columnCount(); ++i) {
                if (const auto arity = editor->columnArity(i)) {
                    mColumns.append({
                        index,
                        offset,
                        editor->columnType(i),
                        arity,
                        editor->columnName(i),
                        true });

                    index += arity;
                    offset += getTypeSize(editor->columnType(i)) * arity;
                }
                // fill padding with not editable column sets
                if (const auto padding = editor->columnPadding(i)) {
                    mColumns.append({
                        index,
                        offset,
                        DataType::Uint8,
                        padding,
                        "",
                        false });

                    index += padding;
                    offset += padding;
                }
            }
            mColumnCount = index;
        }

        int getColumnSize(int index) const
        {
            if (auto column = getColumn(index))
                return getTypeSize(column->type);
            return 0;
        }

        int rowCount(const QModelIndex&) const override
        {
            return mRowCount;
        }

        int columnCount(const QModelIndex&) const override
        {
            return mColumnCount;
        }

        QVariant data(const QModelIndex &index, int role) const override
        {
            if (!index.isValid() ||
                (role != Qt::DisplayRole &&
                 role != Qt::EditRole &&
                 role != Qt::TextAlignmentRole &&
                 role != Qt::UserRole))
            return QVariant();

            auto column = getColumn(index.column());
            if (!column)
                return QVariant();

            if (role == Qt::UserRole)
                return static_cast<int>(column->type);

            if (role == Qt::TextAlignmentRole) {
                if (column->editable)
                    return Qt::AlignRight + Qt::AlignVCenter;
                return Qt::AlignHCenter + Qt::AlignVCenter;
            }

            if (role == Qt::DisplayRole || role == Qt::EditRole) {
                auto columnOffset = getColumnOffset(index.column());
                auto offset = mOffset + mStride * index.row() + columnOffset;
                auto columnSize = getTypeSize(column->type);
                if (columnOffset + columnSize > mStride ||
                    offset + columnSize > mFile.size())
                    return QVariant();

                if (!column->editable)
                    return toHexString(mFile.getByte(offset), 2);

                return mFile.getData(offset, column->type);
            }
            return QVariant();
        }

        bool setData(const QModelIndex &index,
                const QVariant &variant, int role) override
        {
            if (role == Qt::EditRole) {
                const auto column = getColumn(index.column());
                if (!column)
                    return false;

                const auto columnOffset = getColumnOffset(index.column());
                const auto columnSize = getTypeSize(column->type);
                if (columnOffset + columnSize > mStride)
                    return false;

                const auto offset = mOffset + mStride * index.row();
                mFile.expand(offset + mStride);

                mFile.setData(offset + columnOffset, column->type, variant);
            }
            return true;
        }

        QVariant headerData(int section,
            Qt::Orientation orientation, int role) const override
        {
            if (role == Qt::TextAlignmentRole)
                return Qt::AlignLeft + Qt::AlignVCenter;

            if (role == Qt::DisplayRole && orientation == Qt::Vertical)
                return section;

            if ((role == Qt::DisplayRole || role == Qt::ToolTipRole) &&
                 orientation == Qt::Horizontal) {
                auto column = getColumn(section);
                if (column && column->editable) {
                    const auto index = section - column->index;
                    return column->name + (column->arity > 1 ?
                        QString("[%1]").arg(index) : "");
                }
            }

            return QVariant();
        }

        Qt::ItemFlags flags(const QModelIndex &index) const override
        {
            if (auto set = getColumn(index.column()))
                if (set->editable)
                    return Qt::ItemIsEditable |
                           Qt::ItemIsSelectable |
                           Qt::ItemIsEnabled;
            return { };
        }
    };

    class DataEditorDelegate : public QItemDelegate
    {
        QWidget *mDataEditor;
    public:
        DataEditorDelegate(QWidget *editor, QWidget *parent)
            : QItemDelegate(parent)
            , mDataEditor(editor)
        {
        }

        QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
            const QModelIndex &) const override
        {
            mDataEditor->setParent(parent);
            return mDataEditor;
        }

        void destroyEditor(QWidget *editor, const QModelIndex &) const override
        {
            editor->setParent(static_cast<QWidget*>(parent()));
        }

        void updateEditorGeometry(QWidget *editor,
            const QStyleOptionViewItem &option,
            const QModelIndex &) const override
        {
            editor->setGeometry(option.rect);
        }
    };

    char guessSeparator(const QString &text)
    {
        const auto commas = text.count(',');
        const auto tabs = text.count('\t');
        const auto semicolons = text.count(';');
        if (commas > tabs)
            return (commas > semicolons ? ',' : ';');
        else
            return (tabs > semicolons ? '\t' : ';');
    }
} // namespace

//-----------------------------------------------------------------------------

class BinaryEditor::DataEditor : public QTableView
{
public:
    DataEditor(int columnWidth, int rowHeight, QWidget *parent)
        : QTableView(parent)
        , mContextMenu(new QMenu(this))
    {
        setFrameShape(QFrame::NoFrame);
        setAutoScroll(false);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setWordWrap(false);
        setTextElideMode(Qt::ElideNone);
        setAlternatingRowColors(true);
        setSelectionMode(QAbstractItemView::ContiguousSelection);
        setContextMenuPolicy(Qt::CustomContextMenu);

        horizontalHeader()->setDefaultSectionSize(columnWidth);
        horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        horizontalHeader()->setHighlightSections(false);

        verticalHeader()->setDefaultSectionSize(rowHeight);
        verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        verticalHeader()->setHighlightSections(false);

        setItemDelegate(new SpinBoxDelegate(this));

        connect(this, &DataEditor::customContextMenuRequested,
            this, &DataEditor::openContextMenu);
    }

    QList<QMetaObject::Connection> connectEditActions(const EditActions &actions)
    {
        auto c = QList<QMetaObject::Connection>();
        //c += connect(actions.undo, &QAction::triggered, this, &DataEditor::undo);
        //c += connect(actions.redo, &QAction::triggered, this, &DataEditor::redo);
        c += connect(actions.cut, &QAction::triggered, this, &DataEditor::cut);
        c += connect(actions.copy, &QAction::triggered, this, &DataEditor::copy);
        c += connect(actions.paste, &QAction::triggered, this, &DataEditor::paste);
        c += connect(actions.delete_, &QAction::triggered, this, &DataEditor::delete_);
        //c += connect(actions.selectAll, &QAction::triggered, this, &DataEditor::selectAll);

        actions.cut->setEnabled(true);
        actions.copy->setEnabled(true);
        actions.paste->setEnabled(canPaste());
        actions.delete_->setEnabled(true);

        c += connect(QApplication::clipboard(), &QClipboard::changed,
            [this, actions]() { actions.paste->setEnabled(canPaste()); });

        mContextMenu->clear();
        mContextMenu->addAction(actions.undo);
        mContextMenu->addAction(actions.redo);
        mContextMenu->addSeparator();
        mContextMenu->addAction(actions.cut);
        mContextMenu->addAction(actions.copy);
        mContextMenu->addAction(actions.paste);
        mContextMenu->addAction(actions.delete_);
        mContextMenu->addSeparator();
        return c;
    }

    void openContextMenu(const QPoint &pos)
    {
        mContextMenu->popup(mapToGlobal(pos));
    }

    void copy()
    {
        auto str = QString();
        auto prevRow = -1;
        auto newLine = true;
        for (const auto &index : selectedIndexes()) {
            auto row = index.row();
            if (prevRow != -1 && row != prevRow) {
                str += "\n";
                newLine = true;
            }
            if (!newLine)
                str += "\t";
            str += index.data().toString();
            newLine = false;
            prevRow = row;
        }
        QApplication::clipboard()->setText(str);
    }

    bool canPaste() const
    {
        const auto text = QApplication::clipboard()->text();
        return !text.isEmpty();
    }

    void paste()
    {
        if (selectedIndexes().isEmpty())
            return;

        const auto text = QApplication::clipboard()->text();
        const auto separator = guessSeparator(text);
        const auto begin = selectedIndexes().first();
        auto r = begin.row();
        auto c = begin.column();
        foreach (QString row, text.split('\n')) {
            c = begin.column();
            foreach (QString value, row.split(separator)) {
                const auto index = model()->index(r, c);
                if (index.flags() & Qt::ItemIsEditable) {
                    if (value.endsWith('f'))
                        value.remove('f');
                    model()->setData(index, value);
                }
                ++c;
            }
            ++r;
        }
        model()->dataChanged(begin, model()->index(r, c));
    }

    void cut()
    {
        copy();
        delete_();
    }

    void delete_()
    {
        auto minIndex = selectedIndexes().first();
        auto maxIndex = selectedIndexes().first();
        foreach (QModelIndex index, selectedIndexes())
            if (index.flags() & Qt::ItemIsEditable) {
                model()->setData(index, 0.0);
                minIndex = model()->index(
                    std::min(index.row(), minIndex.row()),
                    std::min(index.column(), minIndex.column()));
                maxIndex = model()->index(
                    std::max(index.row(), maxIndex.row()),
                    std::max(index.column(), maxIndex.column()));
            }
        model()->dataChanged(minIndex, maxIndex);
    }

private:
    QMenu *mContextMenu{ };
};

//-----------------------------------------------------------------------------

BinaryEditor::BinaryEditor(BinaryFilePtr file, QWidget *parent)
    : QTableView(parent)
    , mFile(file)
{
    horizontalHeader()->setVisible(false);
    horizontalHeader()->setDefaultSectionSize(mColumnWidth);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(mRowHeight);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    verticalHeader()->setSectionsClickable(false);
    setGridStyle(Qt::DotLine);
    setFocusPolicy(Qt::NoFocus);

    mDataEditor = new DataEditor(mColumnWidth, mRowHeight, this);
    setItemDelegate(new DataEditorDelegate(mDataEditor, this));

    mDataEditor->setStyleSheet(
        "QTableView { margin-bottom: -1px; margin-right: -1px; }");

    refresh();
}

QList<QMetaObject::Connection> BinaryEditor::connectEditActions(
    const EditActions &actions)
{
    auto c = mDataEditor->connectEditActions(actions);

    actions.windowFileName->setText(mFile->fileName());
    actions.windowFileName->setEnabled(mFile->isModified());
    c += connect(mFile.data(), &BinaryFile::fileNameChanged,
        actions.windowFileName, &QAction::setText);
    c += connect(mFile.data(), &BinaryFile::modificationChanged,
        actions.windowFileName, &QAction::setEnabled);

    return c;
}

void BinaryEditor::refresh()
{
    if (!mInvalidated)
        return;
    mInvalidated = false;

    auto prevModel = model();
    setModel(new HexModel(this, &mFile->data()));
    delete prevModel;

    clearSpans();
    setRowHeight(mPrevFirstRow, mRowHeight);

    const auto row = (mOffset + mStride - 1) / mStride;
    auto rowLength = 0;
    for (const auto &column : mColumns)
        rowLength += column.arity * getTypeSize(column.type) + column.padding;

    const auto empty = (!rowLength || !mRowCount);
    mDataEditor->setVisible(!empty);
    if (!empty) {
        setSpan(row, 0, mRowCount, rowLength);

        auto dataModel = new DataModel(this, mFile.data());
        prevModel = mDataEditor->model();
        mDataEditor->setModel(dataModel);
        delete prevModel;

        for (auto i = 0; i < dataModel->columnCount({ }); ++i)
            mDataEditor->horizontalHeader()->resizeSection(i,
                dataModel->getColumnSize(i) * mColumnWidth);

        setRowHeight(row,
            mRowHeight + mDataEditor->horizontalHeader()->height());
        mPrevFirstRow = row;

        openPersistentEditor(model()->index(row, 0));

        setColumnWidth(0,
            mColumnWidth + mDataEditor->verticalHeader()->width());
    }
}

void BinaryEditor::setOffset(int offset)
{
    update(mOffset, offset);
}

void BinaryEditor::setStride(int stride)
{
    if (stride <= 0) {
        stride = 0;
        for (const auto &column : mColumns)
            stride += column.arity * getTypeSize(column.type) + column.padding;
    }
    update(mStride, (stride ? stride : 16));
}

void BinaryEditor::setRowCount(int rowCount)
{
    update(mRowCount, rowCount);
}

void BinaryEditor::setColumnCount(int count)
{
    mInvalidated |= (mColumns.size() != count);
    while (mColumns.size() > count)
        mColumns.pop_back();
    while (mColumns.size() < count)
        mColumns.push_back({ });
}

void BinaryEditor::setColumnName(int index, QString name)
{
    if (auto column = getColumn(index))
        update(column->name, name);
}

void BinaryEditor::setColumnType(int index, DataType type)
{
    if (auto column = getColumn(index))
        update(column->type, type);
}

void BinaryEditor::setColumnArity(int index, int arity)
{
    if (auto column = getColumn(index))
        update(column->arity, arity);
}

void BinaryEditor::setColumnPadding(int index, int padding)
{
    if (auto column = getColumn(index))
        update(column->padding, padding);
}
