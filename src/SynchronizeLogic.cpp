#include "SynchronizeLogic.h"
#include "Singletons.h"
#include "FileCache.h"
#include "VideoManager.h"
#include "session/SessionModel.h"
#include "scripting/ScriptEngine.h"
#include "editors/EditorManager.h"
#include "editors/BinaryEditor.h"
#include "render/RenderSession.h"
#include "render/ProcessSource.h"
#include "render/CompositorSync.h"
#include <QTimer>

SynchronizeLogic::SynchronizeLogic(QObject *parent)
    : QObject(parent)
    , mModel(Singletons::sessionModel())
    , mUpdateEditorsTimer(new QTimer(this))
    , mEvaluationTimer(new QTimer(this))
    , mProcessSourceTimer(new QTimer(this))
    , mProcessSource(new ProcessSource(this))
{
    connect(mUpdateEditorsTimer, &QTimer::timeout,
        this, &SynchronizeLogic::updateEditors);
    connect(mEvaluationTimer, &QTimer::timeout,
        this, &SynchronizeLogic::handleEvaluateTimout);
    connect(mProcessSourceTimer, &QTimer::timeout,
        this, &SynchronizeLogic::processSource);
    connect(&mModel, &SessionModel::dataChanged,
        this, &SynchronizeLogic::handleItemsModified);
    connect(&mModel, &SessionModel::rowsInserted,
        this, &SynchronizeLogic::handleItemReordered);
    connect(&mModel, &SessionModel::rowsAboutToBeRemoved,
        this, &SynchronizeLogic::handleItemReordered);
    connect(mProcessSource, &ProcessSource::outputChanged,
        this, &SynchronizeLogic::outputChanged);
    connect(&Singletons::fileCache(), &FileCache::fileChanged,
        this, &SynchronizeLogic::handleFileChanged);
    connect(&Singletons::editorManager(), &EditorManager::editorRenamed,
        this, &SynchronizeLogic::handleEditorFileRenamed);

    resetRenderSession();

    mUpdateEditorsTimer->start(100);

    mProcessSourceTimer->setInterval(50);
    mProcessSourceTimer->setSingleShot(true);
}

SynchronizeLogic::~SynchronizeLogic() = default;

void SynchronizeLogic::setValidateSource(bool validate)
{
    if (std::exchange(mValidateSource, validate) != validate)
        mProcessSourceTimer->start();
}

void SynchronizeLogic::setProcessSourceType(QString type)
{
    if (std::exchange(mProcessSourceType, type) != type)
        mProcessSourceTimer->start();
}

void SynchronizeLogic::setCurrentEditorFileName(QString fileName)
{
    if (std::exchange(mCurrentEditorFileName, fileName) != fileName)
        mProcessSourceTimer->start();
}

void SynchronizeLogic::setCurrentEditorSourceType(SourceType sourceType)
{
    if (std::exchange(mCurrentEditorSourceType, sourceType) != sourceType)
        mProcessSourceTimer->start();
}

void SynchronizeLogic::resetRenderSession()
{
    mRenderSession.reset(new RenderSession());
    connect(mRenderSession.data(), &RenderTask::updated,
        this, &SynchronizeLogic::handleSessionRendered);
}

void SynchronizeLogic::resetEvaluation()
{
    evaluate(EvaluationType::Reset);
    Singletons::videoManager().rewindVideoFiles();
}

void SynchronizeLogic::manualEvaluation()
{
    evaluate(EvaluationType::Manual);
}

void SynchronizeLogic::setEvaluationMode(EvaluationMode mode)
{
    if (mEvaluationMode == mode)
        return;

    // manual evaluation after steady evaluation
    if (mEvaluationMode == EvaluationMode::Steady) {
        manualEvaluation();
    }

    mEvaluationMode = mode;

    if (mEvaluationMode == EvaluationMode::Steady) {
        mEvaluationTimer->setSingleShot(false);
        mEvaluationTimer->start(10);
        Singletons::videoManager().playVideoFiles();
    }
    else if (mEvaluationMode == EvaluationMode::Automatic) {
        mEvaluationTimer->stop();
        mEvaluationTimer->setSingleShot(true);
        if (mRenderSessionInvalidated)
            mEvaluationTimer->start(0);
        Singletons::videoManager().pauseVideoFiles();
    }
    else {
        mEvaluationTimer->stop();
        Singletons::sessionModel().setActiveItems({ });
        Singletons::videoManager().pauseVideoFiles();
    }
}

void SynchronizeLogic::handleSessionRendered()
{
    Singletons::fileCache().updateEditorFiles();

    if (mEvaluationMode != EvaluationMode::Paused)
        Singletons::sessionModel().setActiveItems(mRenderSession->usedItems());

    if (mEvaluationMode == EvaluationMode::Steady &&
        synchronizeToCompositor())
        mEvaluationTimer->setInterval(1);
}

void SynchronizeLogic::handleFileChanged(const QString &fileName)
{
    mModel.forEachFileItem(
        [&](const FileItem &item) {
            if (item.fileName == fileName) {
                auto index = mModel.getIndex(&item);
                Q_EMIT mModel.dataChanged(index, index);
            }
        });

    auto &editorManager = Singletons::editorManager();
    if (editorManager.currentEditorFileName() == fileName)
        mProcessSourceTimer->start();
}

void SynchronizeLogic::handleItemsModified(const QModelIndex &topLeft,
    const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_UNUSED(bottomRight)
    // ignore ForegroundRole...
    if (roles.empty())
        handleItemModified(topLeft);
}

void SynchronizeLogic::handleItemModified(const QModelIndex &index)
{
    if (auto fileItem = mModel.item<FileItem>(index)) {
        if (index.column() == SessionModel::Name)
            handleFileItemRenamed(*fileItem);
        else if (index.column() == SessionModel::FileName)
            handleFileItemFileChanged(*fileItem);
    }

    if (index.column() != SessionModel::None) {
        if (auto buffer = mModel.item<Buffer>(index)) {
            mEditorItemsModified.insert(buffer->id);
        }
        else if (auto block = mModel.item<Block>(index)) {
            block->evaluatedOffset = -1;
            block->evaluatedRowCount = -1;
            mEditorItemsModified.insert(block->parent->id);
        }
        else if (auto field = mModel.item<Field>(index)) {
            mEditorItemsModified.insert(field->parent->parent->id);
        }
        else if (auto texture = mModel.item<Texture>(index)) {
            mEditorItemsModified.insert(texture->id);
        }
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

    if (mEvaluationMode == EvaluationMode::Automatic)
        mEvaluationTimer->start(50);
}

void SynchronizeLogic::handleItemReordered(const QModelIndex &parent, int first)
{
    mRenderSessionInvalidated = true;
    handleItemModified(mModel.index(first, 0, parent));
}

void SynchronizeLogic::handleEditorFileRenamed(const QString &prevFileName,
    const QString &fileName)
{
    // update item filenames
    mModel.forEachFileItem([&](const FileItem &item) {
        if (item.fileName == prevFileName)
            if (!fileName.isEmpty() || FileDialog::isUntitled(item.fileName))
                mModel.setData(mModel.getIndex(&item, SessionModel::FileName),
                    fileName);
    });
}

void SynchronizeLogic::handleFileItemFileChanged(const FileItem &item)
{
    // update item name
    const auto name = FileDialog::getFileTitle(item.fileName);
    if (name != item.name)
        mModel.setData(mModel.getIndex(&item, SessionModel::Name), name);
}

void SynchronizeLogic::handleFileItemRenamed(const FileItem &item)
{
    if (item.fileName.isEmpty() ||
        FileDialog::getFileTitle(item.fileName) == item.name)
        return;

    mModel.beginUndoMacro("Update filename");

    const auto prevFileName = item.fileName;
    if (!FileDialog::isEmptyOrUntitled(item.fileName)) {
        const auto fileName = QFileInfo(item.fileName).dir().filePath(item.name);
        const auto file = QFileInfo(fileName);
        const auto prevFile = QFileInfo(prevFileName);

        const auto identical = [](const QFileInfo &a, const QFileInfo &b) {
            if (!a.exists() || !b.exists() || a.size() != b.size())
                return false;
            return (QFile(a.fileName()).readAll() == QFile(b.fileName()).readAll());
        };

        const auto mergeRenameOrCopy = [&]() {
            if (identical(prevFile, file))
                return QFile::remove(prevFileName);

            auto fileReferencedByOtherItem = false;
            mModel.forEachFileItem([&](const FileItem& other) {
                if (&item != &other && item.fileName == other.fileName)
                    fileReferencedByOtherItem = true;
            });
            if (!fileReferencedByOtherItem)
                return QFile::rename(prevFileName, fileName);

            return QFile::copy(prevFileName, fileName);
        };
        if (prevFile.exists() && !mergeRenameOrCopy()) {
            // failed, rename item back
            const auto name = QFileInfo(item.fileName).fileName();
            mModel.setData(mModel.getIndex(&item, SessionModel::Name), name);
            return;
        }

        // update item filename
        mModel.setData(mModel.getIndex(&item, SessionModel::FileName), fileName);
    }
    else {
        // update item filename
        const auto fileName = FileDialog::generateNextUntitledFileName(item.name);
        mModel.setData(mModel.getIndex(&item, SessionModel::FileName), fileName);
    }

    mModel.endUndoMacro();

    // rename editor filenames
    Singletons::editorManager().renameEditors(prevFileName, item.fileName);
}

void SynchronizeLogic::handleEvaluateTimout()
{
    evaluate(mEvaluationMode == EvaluationMode::Automatic ?
        EvaluationType::Automatic : EvaluationType::Steady);
}

void SynchronizeLogic::evaluate(EvaluationType evaluationType)
{
    Singletons::fileCache().updateEditorFiles();
    const auto itemsChanged = std::exchange(mRenderSessionInvalidated, false);
    mRenderSession->update(itemsChanged, evaluationType);
}

void SynchronizeLogic::updateEditors()
{
    for (auto itemId : qAsConst(mEditorItemsModified))
        updateEditor(itemId, false);
    mEditorItemsModified.clear();
}

void SynchronizeLogic::updateEditor(ItemId itemId, bool activated)
{
    auto &editors = Singletons::editorManager();
    if (auto texture = mModel.findItem<Texture>(itemId)) {
        if (auto editor = editors.getTextureEditor(texture->fileName))
            updateTextureEditor(*texture, *editor);
    }
    else if (auto buffer = mModel.findItem<Buffer>(itemId)) {
        if (auto editor = editors.getBinaryEditor(buffer->fileName)) {
            updateBinaryEditor(*buffer, *editor);
            if (activated)
                editor->scrollToOffset();
        }
    }
}

void SynchronizeLogic::updateTextureEditor(const Texture &texture,
    TextureEditor &editor)
{
    auto ok = true;
    const auto format = TextureEditor::RawFormat{
        texture.target,
        texture.format,
        evaluateIntExpression(texture.width, &ok),
        evaluateIntExpression(texture.height, &ok),
        evaluateIntExpression(texture.depth, &ok),
        evaluateIntExpression(texture.layers, &ok),
        texture.samples,
    };
    editor.setRawFormat(format);
}

void SynchronizeLogic::updateBinaryEditor(const Buffer &buffer,
    BinaryEditor &editor)
{
    const auto getDataType = [](Field::DataType type) {
        switch (type) {
            case Field::DataType::Int8: return BinaryEditor::DataType::Int8;
            case Field::DataType::Int16: return BinaryEditor::DataType::Int16;
            case Field::DataType::Int32: return BinaryEditor::DataType::Int32;
            case Field::DataType::Uint8: return BinaryEditor::DataType::Uint8;
            case Field::DataType::Uint16: return BinaryEditor::DataType::Uint16;
            case Field::DataType::Uint32: return BinaryEditor::DataType::Uint32;
            case Field::DataType::Float: return BinaryEditor::DataType::Float;
            case Field::DataType::Double: return BinaryEditor::DataType::Double;
        }
        return BinaryEditor::DataType::Int8;
    };

    auto ok = true;
    auto blocks = QList<BinaryEditor::Block>();
    for (const auto *item : qAsConst(buffer.items)) {
        const auto &block = static_cast<const Block&>(*item);
        auto fields = QList<BinaryEditor::Field>();
        for (const auto *item : qAsConst(block.items)) {
            const auto &field = static_cast<const Field&>(*item);
            fields.append({
                field.name,
                getDataType(field.dataType),
                field.count,
                field.padding
            });
        }
        const auto offset = (block.evaluatedOffset >= 0 ? block.evaluatedOffset : 
            evaluateIntExpression(block.offset, &ok));
        const auto rowCount = (block.evaluatedRowCount >= 0 ? block.evaluatedRowCount :
            evaluateIntExpression(block.rowCount, &ok));
        blocks.append({
            block.name,
            offset,
            rowCount,
            fields
        });
    }
    if (ok)
        editor.setBlocks(blocks);
}

void SynchronizeLogic::processSource()
{
    if (!mValidateSource && mProcessSourceType.isEmpty())
        return;

    if (mCurrentEditorFileName.isEmpty() ||
        !Singletons::editorManager().getSourceEditor(mCurrentEditorFileName))
        return;

    Singletons::fileCache().updateEditorFiles();

    mProcessSource->setFileName(mCurrentEditorFileName);
    mProcessSource->setSourceType(mCurrentEditorSourceType);
    mProcessSource->setValidateSource(mValidateSource);
    mProcessSource->setProcessType(mProcessSourceType);
    mProcessSource->update();
}
