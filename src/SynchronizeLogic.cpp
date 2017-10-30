#include "SynchronizeLogic.h"
#include "Singletons.h"
#include "FileCache.h"
#include "session/SessionModel.h"
#include "editors/EditorManager.h"
#include "editors/BinaryEditor.h"
#include "render/RenderSession.h"
#include "render/ValidateSource.h"
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
    , mEvaluationTimer(new QTimer(this))
    , mValidateSourceTimer(new QTimer(this))
{
    connect(mEvaluationTimer, &QTimer::timeout,
        [this]() { evaluate(); });
    connect(mValidateSourceTimer, &QTimer::timeout,
        this, &SynchronizeLogic::validateSource);
    connect(&mModel, &SessionModel::dataChanged,
        this, &SynchronizeLogic::handleItemModified);
    connect(&mModel, &SessionModel::rowsInserted,
        this, &SynchronizeLogic::handleItemReordered);
    connect(&mModel, &SessionModel::rowsAboutToBeRemoved,
        this, &SynchronizeLogic::handleItemReordered);

    resetRenderSession();
    setEvaluationMode(false, false);

    mValidateSourceTimer->setInterval(500);
    mValidateSourceTimer->setSingleShot(true);
}

SynchronizeLogic::~SynchronizeLogic() = default;

void SynchronizeLogic::setSourceValidationActive(bool active)
{
    if (active) {
        if (!mValidateSource) {
            mValidateSource.reset(new ValidateSource());
            validateSource();
        }
    }
    else {
        mValidateSource.reset();
    }
}

void SynchronizeLogic::resetRenderSession()
{
    mRenderSession.reset(new RenderSession());
    connect(mRenderSession.data(), &RenderTask::updated,
        this, &SynchronizeLogic::handleSessionRendered);
}

void SynchronizeLogic::manualEvaluation()
{
    mRenderSessionInvalidated = true;
    evaluate(true);
}

void SynchronizeLogic::setEvaluationMode(bool automatic, bool steady)
{
    mAutomaticEvaluation = automatic;
    mSteadyEvaluation = steady;
    mRenderSessionInvalidated = true;

    if (mSteadyEvaluation) {
        mEvaluationTimer->setSingleShot(false);
        mEvaluationTimer->start();
    }
    else if (mAutomaticEvaluation) {
        mEvaluationTimer->setSingleShot(true);
        mEvaluationTimer->stop();
        evaluate();
    }
    else {
        mEvaluationTimer->stop();
        Singletons::sessionModel().setActiveItems({ });
    }
}

void SynchronizeLogic::setEvaluationInterval(int interval)
{
    mEvaluationTimer->setInterval(interval);
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
                auto index = mModel.getIndex(&item);
                emit mModel.dataChanged(index, index);
            }
        });

    auto &editorManager = Singletons::editorManager();
    if (editorManager.currentEditorFileName() == fileName)
        mValidateSourceTimer->start();
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
    else if (mModel.item<Binding>(index)) {
        mRenderSessionInvalidated = true;
    }

    mEvaluationTimer->start();
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
                mModel.setData(mModel.getIndex(&item, SessionModel::FileName),
                    fileName);
        });
}

void SynchronizeLogic::handleSourceTypeChanged(SourceType sourceType)
{
    Q_UNUSED(sourceType);
    validateSource();
}

void SynchronizeLogic::evaluate(bool manualEvaluation)
{
    auto &editors = Singletons::editorManager();
    foreach (ItemId bufferId, mBuffersModified)
        if (auto buffer = mModel.findItem<Buffer>(bufferId))
            if (auto editor = editors.getBinaryEditor(buffer->fileName))
                updateBinaryEditor(*buffer, *editor);
    mBuffersModified.clear();

    if (manualEvaluation || mAutomaticEvaluation || mSteadyEvaluation) {
        updateFileCache();
        mRenderSession->update(mRenderSessionInvalidated,
            manualEvaluation, mSteadyEvaluation);
    }
    mRenderSessionInvalidated = false;
}

void SynchronizeLogic::updateFileCache()
{
    auto &editors = Singletons::editorManager();
    Singletons::fileCache().update(editors, mFilesModified);
    mFilesModified.clear();
}

void SynchronizeLogic::updateBinaryEditor(const Buffer &buffer,
    BinaryEditor &editor, bool scrollToOffset)
{
    auto mapDataType = [](Column::DataType type) {
        switch (type) {
            case Column::DataType::Int8: return BinaryEditor::DataType::Int8;
            case Column::DataType::Int16: return BinaryEditor::DataType::Int16;
            case Column::DataType::Int32: return BinaryEditor::DataType::Int32;
            case Column::DataType::Uint8: return BinaryEditor::DataType::Uint8;
            case Column::DataType::Uint16: return BinaryEditor::DataType::Uint16;
            case Column::DataType::Uint32: return BinaryEditor::DataType::Uint32;
            case Column::DataType::Float: return BinaryEditor::DataType::Float;
            case Column::DataType::Double: return BinaryEditor::DataType::Double;
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

    if (scrollToOffset)
        editor.scrollToOffset();
}

void SynchronizeLogic::validateSource()
{
    if (mValidateSource) {
        auto &editorManager = Singletons::editorManager();
        mValidateSource->setSource(
            editorManager.currentEditorFileName(),
            editorManager.currentSourceType());
        updateFileCache();
        mValidateSource->update(false, false, false);
    }
}
