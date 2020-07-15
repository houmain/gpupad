#include "BinaryEditor.h"
#include "BinaryEditor_EditableRegion.h"
#include "BinaryEditor_HexModel.h"
#include "BinaryEditor_DataModel.h"
#include "FileDialog.h"
#include "Singletons.h"
#include "FileCache.h"
#include <QHeaderView>
#include <QSaveFile>

namespace
{
    template <typename T>
    bool set(T &property, const T &value)
    {
        if (property == value)
            return false;
        property = value;
        return true;
    }
} // namespace

int BinaryEditor::getTypeSize(DataType type)
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

BinaryEditor::BinaryEditor(QString fileName, QWidget *parent)
    : QTableView(parent)
    , mFileName(fileName)
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

    mEditableRegion = new EditableRegion(mColumnWidth, mRowHeight, this);
    setItemDelegate(new EditableRegionDelegate(mEditableRegion, this));
    mEditableRegion->setStyleSheet("QTableView { margin: -2px; }");

    setStride();
    refresh();
}

BinaryEditor::~BinaryEditor()
{
    if (isModified())
        Singletons::fileCache().invalidateEditorFile(mFileName);
}

QList<QMetaObject::Connection> BinaryEditor::connectEditActions(
    const EditActions &actions)
{
    auto c = mEditableRegion->connectEditActions(actions);

    actions.windowFileName->setText(fileName());
    actions.windowFileName->setEnabled(isModified());
    c += connect(this, &BinaryEditor::fileNameChanged,
        actions.windowFileName, &QAction::setText);
    c += connect(this, &BinaryEditor::modificationChanged,
        actions.windowFileName, &QAction::setEnabled);

    return c;
}

void BinaryEditor::setFileName(QString fileName)
{
    mFileName = fileName;
    emit fileNameChanged(mFileName);
}

bool BinaryEditor::load(const QString &fileName, QByteArray *data)
{
    if (!data || FileDialog::isEmptyOrUntitled(fileName))
        return false;

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return false;
    *data = file.readAll();
    return true;
}

bool BinaryEditor::load()
{
    auto data = QByteArray();
    if (!Singletons::fileCache().getBinary(mFileName, &data))
        return false;

    replace(data);
    setModified(false);
    return true;
}

bool BinaryEditor::reload()
{
    auto data = QByteArray();
    if (!load(mFileName, &data))
        return false;

    replace(data);
    setModified(false);
    return true;
}

bool BinaryEditor::save()
{
    QSaveFile file(fileName());
    if (!file.open(QFile::WriteOnly | QFile::Truncate))
        return false;
    file.write(mData);
    if (!file.commit())
        return false;

    setModified(false);
    return true;
}

void BinaryEditor::replace(QByteArray data, bool invalidateFileCache)
{
    if (data.isSharedWith(mData))
        return;

    mData = data;
    refresh();

    if (!FileDialog::isEmptyOrUntitled(mFileName))
        setModified(true);

    if (invalidateFileCache)
        Singletons::fileCache().invalidateEditorFile(mFileName);
}

void BinaryEditor::handleDataChanged()
{
    setModified(true);
    Singletons::fileCache().invalidateEditorFile(mFileName);
}

void BinaryEditor::setModified(bool modified)
{
    if (mModified != modified) {
        mModified = modified;
        emit modificationChanged(modified);
    }
}

auto BinaryEditor::getColumn(int index) -> Column*
{
    return (index < mColumns.size() ? &mColumns[index] : nullptr);
}

auto BinaryEditor::getColumn(int index) const -> Column
{
    return (index < mColumns.size() ? mColumns[index] : Column{ });
}

void BinaryEditor::refresh()
{
    const auto row = (mOffset + mStride - 1) / mStride;
    auto rowLength = 0;
    for (const auto &column : qAsConst(mColumns))
        rowLength += column.arity * getTypeSize(column.type) + column.padding;
    const auto rowCount = (rowLength ? mRowCount : 0);
    const auto offset = (rowLength ? mOffset : 0);

    auto prevModel = model();
    setModel(new HexModel(&mData, offset, mStride, rowCount, this));
    delete prevModel;

    clearSpans();
    setRowHeight(mPrevFirstRow, mRowHeight);

    const auto empty = (!rowCount);
    mEditableRegion->setVisible(!empty);
    if (!empty) {
        setSpan(row, 0, mRowCount, rowLength);

        auto dataModel = new DataModel(this, &mData);
        prevModel = mEditableRegion->model();
        mEditableRegion->setModel(dataModel);
        delete prevModel;

        mEditableRegion->horizontalHeader()->setMinimumSectionSize(1);
        for (auto i = 0; i < dataModel->columnCount({ }); ++i)
            mEditableRegion->horizontalHeader()->resizeSection(i,
                dataModel->getColumnSize(i) * mColumnWidth);

        setRowHeight(row,
            mRowHeight + mEditableRegion->horizontalHeader()->height() + 1);
        mPrevFirstRow = row;

        openPersistentEditor(model()->index(row, 0));

        setColumnWidth(0,
            mColumnWidth + mEditableRegion->verticalHeader()->width());
    }
}

void BinaryEditor::scrollToOffset()
{
    const auto row = (mOffset + mStride - 1) / mStride;
    scrollTo(model()->index(row, 0));
}

void BinaryEditor::setOffset(int offset)
{
    mColumnsInvalidated |= set(mOffset, offset);
}

void BinaryEditor::setStride(int stride)
{
    if (stride <= 0) {
        stride = 0;
        for (const auto &column : qAsConst(mColumns))
            stride += column.arity * getTypeSize(column.type) + column.padding;
    }
    mColumnsInvalidated |= set(mStride, (stride ? stride : 16));
}

void BinaryEditor::setRowCount(int rowCount)
{
    mColumnsInvalidated |= set(mRowCount, rowCount);
}

void BinaryEditor::setColumnCount(int count)
{
    mColumnsInvalidated |= (mColumns.size() != count);
    while (mColumns.size() > count)
        mColumns.pop_back();
    while (mColumns.size() < count)
        mColumns.push_back({ });
}

void BinaryEditor::setColumnName(int index, QString name)
{
    if (auto column = getColumn(index))
        mColumnsInvalidated |= set(column->name, name);
}

void BinaryEditor::setColumnType(int index, DataType type)
{
    if (auto column = getColumn(index))
        mColumnsInvalidated |= set(column->type, type);
}

void BinaryEditor::setColumnArity(int index, int arity)
{
    if (auto column = getColumn(index))
        mColumnsInvalidated |= set(column->arity, arity);
}

void BinaryEditor::setColumnPadding(int index, int padding)
{
    if (auto column = getColumn(index))
        mColumnsInvalidated |= set(column->padding, padding);
}

void BinaryEditor::updateColumns()
{
    if (std::exchange(mColumnsInvalidated, false))
        refresh();
}

void BinaryEditor::wheelEvent(QWheelEvent *event)
{
    setFocus();
    QTableView::wheelEvent(event);
}
