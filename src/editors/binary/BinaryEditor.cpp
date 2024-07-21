#include "BinaryEditor.h"
#include "BinaryEditorToolBar.h"
#include "BinaryEditor_HexModel.h"
#include "BinaryEditor_DataModel.h"
#include "BinaryEditor_EditableRegion.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "Singletons.h"
#include <QHeaderView>
#include <QSaveFile>
#include <cstdint>
#include <cstring>

namespace {
    template <typename T>
    bool set(T &property, const T &value)
    {
        if (property == value)
            return false;
        property = value;
        return true;
    }
} // namespace

[[maybe_unused]] bool operator==(const BinaryEditor::Field &a,
    const BinaryEditor::Field &b)
{
    return std::tie(a.name, a.dataType, a.count, a.padding)
        == std::tie(b.name, b.dataType, b.count, b.padding);
}

[[maybe_unused]] bool operator==(const BinaryEditor::Block &a,
    const BinaryEditor::Block &b)
{
    return std::tie(a.name, a.offset, a.rowCount, a.fields)
        == std::tie(b.name, b.offset, b.rowCount, b.fields);
}

int BinaryEditor::getTypeSize(DataType type)
{
    switch (type) {
    case DataType::Int8:   return 1;
    case DataType::Int16:  return 2;
    case DataType::Int32:  return 4;
    case DataType::Int64:  return 8;
    case DataType::Uint8:  return 1;
    case DataType::Uint16: return 2;
    case DataType::Uint32: return 4;
    case DataType::Uint64: return 8;
    case DataType::Float:  return 4;
    case DataType::Double: return 8;
    }
    return 0;
}

int BinaryEditor::getStride(const Block &block)
{
    auto stride = 0;
    for (const auto &field : block.fields)
        stride += getTypeSize(field.dataType) * field.count + field.padding;
    return stride;
}

BinaryEditor::BinaryEditor(QString fileName, BinaryEditorToolBar *editorToolbar,
    QWidget *parent)
    : QTableView(parent)
    , mEditorToolBar(*editorToolbar)
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

    mEditableRegion = new EditableRegion(mColumnWidth, mRowHeight, this);
    setItemDelegate(new EditableRegionDelegate(mEditableRegion, this));
    mEditableRegion->setStyleSheet("QTableView { margin: -2px; }");

    refresh();
}

BinaryEditor::~BinaryEditor()
{
    Singletons::fileCache().invalidateFile(mFileName);
}

QList<QMetaObject::Connection> BinaryEditor::connectEditActions(
    const EditActions &actions)
{
    auto c = mEditableRegion->connectEditActions(actions);

    actions.windowFileName->setText(fileName());
    actions.windowFileName->setEnabled(isModified());
    c += connect(this, &BinaryEditor::fileNameChanged, actions.windowFileName,
        &QAction::setText);
    c += connect(this, &BinaryEditor::modificationChanged,
        actions.windowFileName, &QAction::setEnabled);

    updateEditorToolBar();

    c += connect(&mEditorToolBar, &BinaryEditorToolBar::blockIndexChanged, this,
        &BinaryEditor::setCurrentBlockIndex);

    return c;
}

void BinaryEditor::setFileName(QString fileName)
{
    if (mFileName != fileName) {
        mFileName = fileName;
        Q_EMIT fileNameChanged(mFileName);
    }
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

void BinaryEditor::replace(QByteArray data, bool emitFileChanged)
{
    if (data.isSharedWith(mData))
        return;

    mData = data;
    refresh();

    if (!FileDialog::isEmptyOrUntitled(mFileName))
        setModified(true);

    Singletons::fileCache().handleEditorFileChanged(mFileName, emitFileChanged);
}

void BinaryEditor::replaceRange(int offset, QByteArray data,
    bool emitFileChanged)
{
    if (offset == 0 && data.size() >= mData.size()) {
        mData = data;
    } else {
        if (offset + data.size() > mData.size())
            mData.resize(offset + data.size());
        std::memcpy(mData.data() + offset, data.constData(), data.size());
    }
    replace(std::exchange(mData, {}), emitFileChanged);
}

void BinaryEditor::handleDataChanged()
{
    setModified(true);
    Singletons::fileCache().handleEditorFileChanged(mFileName);
}

void BinaryEditor::setModified()
{
    setModified(true);
}

void BinaryEditor::setModified(bool modified)
{
    if (mModified != modified) {
        mModified = modified;
        Q_EMIT modificationChanged(modified);
    }
}

auto BinaryEditor::currentBlock() const -> const Block *
{
    return (mBlocks.empty() ? nullptr
                            : &mBlocks[std::min(mCurrentBlockIndex,
                                  static_cast<int>(mBlocks.size()) - 1)]);
}

void BinaryEditor::setCurrentBlockIndex(int index)
{
    if (index != mCurrentBlockIndex) {
        mCurrentBlockIndex = index;
        refresh();
        scrollToOffset();
    }
}

void BinaryEditor::refresh()
{
    auto stride = 0;
    auto rowCount = 0;
    auto offset = 0;

    const auto *block = currentBlock();
    if (block)
        stride = getStride(*block);

    if (stride) {
        rowCount = block->rowCount;
        offset = block->offset;
    } else {
        stride = 16;
    }

    auto prevModel = model();
    setModel(new HexModel(&mData, offset, stride, rowCount, this));
    delete prevModel;

    clearSpans();
    setRowHeight(mPrevFirstRow, mRowHeight);

    const auto empty = (!rowCount);
    mEditableRegion->setVisible(!empty);
    if (!empty) {
        const auto row = (offset + stride - 1) / stride;
        setSpan(row, 0, rowCount, stride);

        auto dataModel = new DataModel(this, *block, &mData);
        prevModel = mEditableRegion->model();
        mEditableRegion->setModel(dataModel);
        delete prevModel;

        connect(dataModel, &DataModel::dataChanged, this,
            &BinaryEditor::handleDataChanged);

        mEditableRegion->horizontalHeader()->setMinimumSectionSize(1);
        for (auto i = 0; i < dataModel->columnCount({}); ++i)
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

void BinaryEditor::updateEditorToolBar()
{
    mEditorToolBar.setBlocks(mBlocks);
    mEditorToolBar.setCurrentBlockIndex(mCurrentBlockIndex);
}

void BinaryEditor::setBlocks(QList<Block> blocks)
{
    if (mBlocks != blocks) {
        mBlocks = blocks;
        refresh();
    }
}

void BinaryEditor::scrollToOffset()
{
    if (const auto *block = currentBlock())
        if (const auto stride = getStride(*block)) {
            const auto row = (block->offset + stride - 1) / stride;
            scrollTo(model()->index(row, 0));
        }
}

void BinaryEditor::wheelEvent(QWheelEvent *event)
{
    setFocus();
    QTableView::wheelEvent(event);
}
