#include "SynchronizeLogic.h"
#include "Singletons.h"
#include "FileCache.h"
#include "session/SessionModel.h"
#include "editors/EditorManager.h"
#include "editors/SourceEditor.h"
#include "editors/BinaryEditor.h"
#include "render/RenderCall.h"
#include <QTimer>

template<typename F> // F(const FileItem&)
void forEachFileItem(SessionModel& model, const F &function) {
    model.forEachItem([&](const Item& item) {
        if (item.itemType == ItemType::Buffer ||
            item.itemType == ItemType::Shader ||
            item.itemType == ItemType::Texture ||
            item.itemType == ItemType::Image ||
            item.itemType == ItemType::Script)
            function(static_cast<const FileItem&>(item));
    });
}

SynchronizeLogic::SynchronizeLogic(
    QObject *parent)
  : QObject(parent)
  , mModel(Singletons::sessionModel())
  , mUpdateTimer(new QTimer(this))
{
    mUpdateTimer->setSingleShot(true);
    mUpdateTimer->setInterval(50);
    connect(mUpdateTimer, &QTimer::timeout,
        this, &SynchronizeLogic::update);

    using Overload = void(SynchronizeLogic::*)(const QModelIndex&);
    connect(&mModel, &SessionModel::dataChanged,
        this, static_cast<Overload>(&SynchronizeLogic::handleSessionChanged));

    using Overload2 = void(SynchronizeLogic::*)(const QModelIndex&, int);
    connect(&mModel, &SessionModel::rowsInserted,
        this, static_cast<Overload2>(&SynchronizeLogic::handleSessionChanged));
    connect(&mModel, &SessionModel::rowsAboutToBeRemoved,
        this, static_cast<Overload2>(&SynchronizeLogic::handleSessionChanged));
}

SynchronizeLogic::~SynchronizeLogic() = default;

void SynchronizeLogic::handleMessageActivated(ItemId item, QString fileName,
        int line, int column)
{
    Q_UNUSED(item);
    Singletons::editorManager().openSourceEditor(fileName, true, line, column);
}

void SynchronizeLogic::handleSessionChanged(const QModelIndex &parent, int first)
{
    handleSessionChanged(mModel.index(first, 0, parent));
}

void SynchronizeLogic::handleSessionChanged(const QModelIndex &index)
{
    if (auto buffer = mModel.item<Buffer>(index)) {
        mBuffersModified.insert(buffer->id);
        mItemsModified.insert(buffer->id);
    }
    else if (auto column = mModel.item<Column>(index)) {
        mBuffersModified.insert(column->parent->id);
        mItemsModified.insert(column->parent->id);
    }
    mItemsModified.insert(mModel.getItemId(index));
    mUpdateTimer->start();
}

void SynchronizeLogic::handleSourceEditorChanged(const QString &fileName)
{
    mEditorsModified.insert(fileName);
    mUpdateTimer->start();
}

void SynchronizeLogic::handleBinaryEditorChanged(const QString &fileName)
{
    mEditorsModified.insert(fileName);
    mUpdateTimer->start();
}

void SynchronizeLogic::handleImageEditorChanged(const QString &fileName)
{
    mEditorsModified.insert(fileName);
    mUpdateTimer->start();
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

void SynchronizeLogic::update()
{
    mUpdateTimer->stop();

    foreach (ItemId bufferId, mBuffersModified)
        if (auto buffer = mModel.findItem<Buffer>(bufferId))
            updateBinaryEditor(*buffer);
    mBuffersModified.clear();

    if (mActiveRenderTask) {
        foreach (ItemId itemId, mItemsModified)
            if (mActiveRenderTask->usedItems().contains(itemId)) {
                Singletons::fileCache().update(Singletons::editorManager());
                mActiveRenderTask->update();
                break;
            }
    }
    mItemsModified.clear();

    foreach (QString fileName, mEditorsModified) {
        handleFileItemsChanged(fileName);
    }
    mEditorsModified.clear();
}

void SynchronizeLogic::deactivateCalls()
{
    mActiveRenderTask.reset();
    Singletons::sessionModel().setActiveItems({ });
}

void SynchronizeLogic::handleItemActivated(const QModelIndex &index)
{
    if (auto buffer = mModel.item<Buffer>(index)) {
        Singletons::editorManager().openBinaryEditor(buffer->fileName);
        updateBinaryEditor(*buffer);
    }
    else if (auto texture = mModel.item<Texture>(index)) {
        Singletons::editorManager().openImageEditor(texture->fileName);
    }
    else if (auto image = mModel.item<Image>(index)) {
        Singletons::editorManager().openImageEditor(image->fileName);
    }
    else if (auto shader = mModel.item<Shader>(index)) {
        Singletons::editorManager().openSourceEditor(shader->fileName);
    }
    else if (auto script = mModel.item<Script>(index)) {
        Singletons::editorManager().openSourceEditor(script->fileName);
    }
    else if (auto call = mModel.item<Call>(index)) {
        Singletons::fileCache().update(Singletons::editorManager());
        mActiveRenderTask.reset(new RenderCall(call->id));
        connect(mActiveRenderTask.data(), &RenderTask::updated,
            this, &SynchronizeLogic::handleTaskRendered);
        mActiveRenderTask->update();
    }
}

void SynchronizeLogic::handleTaskRendered()
{
    if (mActiveRenderTask)
        Singletons::sessionModel().setActiveItems(mActiveRenderTask->usedItems());
}

void SynchronizeLogic::handleFileItemsChanged(const QString &fileName)
{
    forEachFileItem(mModel, [&](const FileItem &item) {
        if (item.fileName == fileName) {
            auto index = mModel.index(&item);
            emit mModel.dataChanged(index, index);
        }
    });
}

void SynchronizeLogic::updateBinaryEditor(const Buffer &buffer)
{
    auto editor = Singletons::editorManager().getBinaryEditor(buffer.fileName);
    if (!editor)
        return;

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

    editor->setColumnCount(buffer.items.size());
    editor->setOffset(buffer.offset);
    editor->setRowCount(buffer.rowCount);
    auto i = 0;
    foreach (Item *item, buffer.items) {
        const auto &column = static_cast<Column&>(*item);
        editor->setColumnName(i, column.name);
        editor->setColumnType(i, mapDataType(column.dataType));
        editor->setColumnArity(i, column.count);
        editor->setColumnPadding(i, column.padding);
        i++;
    }
    editor->setStride();
    editor->updateColumns();
}
