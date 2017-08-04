#include "SynchronizeLogic.h"
#include "Singletons.h"
#include "FileCache.h"
#include "session/SessionModel.h"
#include "editors/EditorManager.h"
#include "editors/SourceEditor.h"
#include "editors/BinaryEditor.h"
#include "render/RenderSession.h"
#include <QTimer>

template<typename F> // F(const FileItem&)
void forEachFileItem(SessionModel& model, const F &function) {
    model.forEachItem([&](const Item& item) {
        if (auto fileItem = castItem<FileItem>(item))
            function(*fileItem);
    });
}

SynchronizeLogic::SynchronizeLogic(QObject *parent)
  : QObject(parent)
  , mModel(Singletons::sessionModel())
  , mUpdateTimer(new QTimer(this))
{
    connect(mUpdateTimer, &QTimer::timeout,
        [this]() { update(); });
    connect(&mModel, &SessionModel::dataChanged,
        this, &SynchronizeLogic::handleItemModified);
    connect(&mModel, &SessionModel::rowsInserted,
        this, &SynchronizeLogic::handleItemReordered);
    connect(&mModel, &SessionModel::rowsAboutToBeRemoved,
        this, &SynchronizeLogic::handleItemReordered);

    resetRenderSession();
    setEvaluationMode(false, false);
}

SynchronizeLogic::~SynchronizeLogic() = default;

void SynchronizeLogic::resetRenderSession()
{
    mRenderSession.reset(new RenderSession());
    connect(mRenderSession.data(), &RenderTask::updated,
        this, &SynchronizeLogic::handleSessionRendered);
}

void SynchronizeLogic::manualEvaluation()
{
    mRenderSessionInvalidated = true;
    update(true);
}

void SynchronizeLogic::setEvaluationMode(bool automatic, bool steady)
{
    mAutomaticEvaluation = automatic;
    mSteadyEvaluation = steady;
    mRenderSessionInvalidated = true;

    if (mSteadyEvaluation) {
        mUpdateTimer->setSingleShot(false);
        mUpdateTimer->start();
    }
    else if (mAutomaticEvaluation) {
        mUpdateTimer->setSingleShot(true);
        mUpdateTimer->stop();
        update();
    }
    else {
        mUpdateTimer->stop();
        Singletons::sessionModel().setActiveItems({ });
    }
}

void SynchronizeLogic::setEvaluationInterval(int interval)
{
    mUpdateTimer->setInterval(interval);
}

void SynchronizeLogic::handleSessionRendered()
{
    if (mAutomaticEvaluation || mSteadyEvaluation)
        Singletons::sessionModel().setActiveItems(mRenderSession->usedItems());
}

void SynchronizeLogic::handleFileItemsChanged(const QString &fileName)
{
    mFilesModified += fileName;

    forEachFileItem(mModel,
        [&](const FileItem &item) {
            if (item.fileName == fileName) {
                auto index = mModel.index(&item);
                emit mModel.dataChanged(index, index);
            }
        });
}

void SynchronizeLogic::handleItemModified(const QModelIndex &index)
{
    if (auto buffer = mModel.item<Buffer>(index)) {
        mBuffersModified.insert(buffer->id);
    }
    else if (auto column = mModel.item<Column>(index)) {
        mBuffersModified.insert(column->parent->id);
    }

    if (mRenderSession->usedItems().contains(mModel.getItemId(index))) {
        mRenderSessionInvalidated = true;
    }
    else if (index.column() == SessionModel::Name) {
        mRenderSessionInvalidated = true;
    }
    else if (auto call = mModel.item<Call>(index)) {
        if (call->checked)
            mRenderSessionInvalidated = true;
    }
    else if (mModel.item<Group>(index)) {
        mRenderSessionInvalidated = true;
    }

    mUpdateTimer->start();
}

void SynchronizeLogic::handleItemReordered(const QModelIndex &parent, int first)
{
    mRenderSessionInvalidated = true;
    handleItemModified(mModel.index(first, 0, parent));
}

void SynchronizeLogic::handleFileRenamed(const QString &prevFileName,
    const QString &fileName)
{
    if (FileDialog::isUntitled(prevFileName))
        forEachFileItem(mModel, [&](const FileItem &item) {
            if (item.fileName == prevFileName)
                mModel.setData(mModel.index(&item, SessionModel::FileName),
                    fileName);
        });
}

void SynchronizeLogic::update(bool manualEvaluation)
{
    auto &editors = Singletons::editorManager();
    foreach (ItemId bufferId, mBuffersModified)
        if (auto buffer = mModel.findItem<Buffer>(bufferId))
            if (auto editor = editors.getBinaryEditor(buffer->fileName))
                updateBinaryEditor(*buffer, *editor);
    mBuffersModified.clear();

    if (manualEvaluation || mAutomaticEvaluation || mSteadyEvaluation) {
        updateFileCache();
        mRenderSession->update(mRenderSessionInvalidated, manualEvaluation);
    }
    mRenderSessionInvalidated = false;
}

void SynchronizeLogic::updateFileCache()
{
    auto &editors = Singletons::editorManager();
    Singletons::fileCache().update(editors, mFilesModified);
    mFilesModified.clear();
}

void SynchronizeLogic::handleItemActivated(const QModelIndex &index,
    bool *handled)
{
    auto &editors = Singletons::editorManager();
    if (auto fileItem = mModel.item<FileItem>(index)) {
        switch (fileItem->itemType) {
            case ItemType::Texture:
            case ItemType::Image:
                if (fileItem->fileName.isEmpty())
                    mModel.setData(mModel.index(fileItem, SessionModel::FileName),
                        editors.openNewImageEditor(fileItem->name));
                editors.openImageEditor(fileItem->fileName);
                break;

            case ItemType::Shader:
            case ItemType::Script:
                if (fileItem->fileName.isEmpty())
                    mModel.setData(mModel.index(fileItem, SessionModel::FileName),
                        editors.openNewSourceEditor(fileItem->name));
                editors.openSourceEditor(fileItem->fileName);
                break;

            case ItemType::Buffer:
                if (fileItem->fileName.isEmpty())
                    mModel.setData(mModel.index(fileItem, SessionModel::FileName),
                        editors.openNewBinaryEditor(fileItem->name));

                if (auto editor = editors.openBinaryEditor(fileItem->fileName)) {
                    updateBinaryEditor(static_cast<const Buffer&>(*fileItem), *editor);
                    editor->scrollToOffset();
                }
                break;

            default:
                break;
        }
        *handled = true;
    }
}

void SynchronizeLogic::updateBinaryEditor(const Buffer &buffer,
    BinaryEditor &editor)
{
    auto mapDataType = [](Column::DataType type) {
        switch (type) {
            case Column::Int8: return BinaryEditor::DataType::Int8;
            case Column::Int16: return BinaryEditor::DataType::Int16;
            case Column::Int32: return BinaryEditor::DataType::Int32;
            case Column::Uint8: return BinaryEditor::DataType::Uint8;
            case Column::Uint16: return BinaryEditor::DataType::Uint16;
            case Column::Uint32: return BinaryEditor::DataType::Uint32;
            case Column::Float: return BinaryEditor::DataType::Float;
            case Column::Double: return BinaryEditor::DataType::Double;
        }
        return BinaryEditor::DataType::Int8;
    };

    editor.setColumnCount(buffer.items.size());
    editor.setOffset(buffer.offset);
    editor.setRowCount(buffer.rowCount);
    auto i = 0;
    foreach (Item *item, buffer.items) {
        const auto &column = static_cast<Column&>(*item);
        editor.setColumnName(i, column.name);
        editor.setColumnType(i, mapDataType(column.dataType));
        editor.setColumnArity(i, column.count);
        editor.setColumnPadding(i, column.padding);
        i++;
    }
    editor.setStride();
    editor.updateColumns();
}
