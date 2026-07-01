#include "SynchronizeLogic.h"
#include "FileCache.h"
#include "Singletons.h"
#include "InputState.h"
#include "VideoManager.h"
#include "editors/EditorManager.h"
#include "editors/binary/BinaryEditor.h"
#include "editors/texture/TextureEditor.h"
#include "editors/source/SourceEditor.h"
#include "render/ProcessSource.h"
#include "render/RenderSessionBase.h"
#include "session/SessionModel.h"
#include "session/properties/PropertiesEditor.h"
#include "scripting/ScriptTimeout.h"
#include "scripting/ScriptEngine.h"
#include <QTimer>
#include <QMetaEnum>

namespace {
    QString unsplitPascalCase(QString string)
    {
        if (string.isEmpty() || string.back().isSpace())
            return string;
        return string.removeIf([](QChar c) { return c.isSpace(); });
    }

    template <typename T>
    bool hasDefaultName(QString name, const QString &defaultName)
    {
        name = unsplitPascalCase(name);
        auto metaType = QMetaEnum::fromType<T>();
        for (auto i = 0; i < metaType.keyCount(); ++i)
            if (name == metaType.key(i))
                return true;
        return (name == defaultName);
    }

    template <typename T>
    QString getValueName(const T &value)
    {
        auto metaType = QMetaEnum::fromType<T>();
        if (auto key = metaType.key(static_cast<int>(value)))
            return splitPascalCase(key);
        return {};
    }

    template <typename T>
    bool hasDefaultName(const Item &item)
    {
        const auto typeName = getValueName(item.type);
        return hasDefaultName<T>(item.name, typeName);
    }
} // namespace

SynchronizeLogic::SynchronizeLogic(QObject *parent)
    : QObject(parent)
    , mModel(Singletons::sessionModel())
    , mUpdateEditorsTimer(new QTimer(this))
    , mEvaluationTimer(new QTimer(this))
    , mProcessSourceTimer(new QTimer(this))
{
    connect(mUpdateEditorsTimer, &QTimer::timeout, this,
        &SynchronizeLogic::updateEditors);
    connect(mEvaluationTimer, &QTimer::timeout, this,
        &SynchronizeLogic::handleEvaluateTimout);
    connect(mProcessSourceTimer, &QTimer::timeout, this,
        &SynchronizeLogic::processSource);
    connect(&mModel, &SessionModel::itemRenamed, this,
        &SynchronizeLogic::handleItemRenamed);
    connect(&mModel, &SessionModel::dataChanged, this,
        &SynchronizeLogic::handleItemsModified);
    connect(&mModel, &SessionModel::rowsInserted, this,
        &SynchronizeLogic::handleItemAdded);
    connect(&mModel, &SessionModel::rowsAboutToBeRemoved, this,
        &SynchronizeLogic::handleItemRemoved);
    connect(&Singletons::fileCache(), &FileCache::fileChanged, this,
        &SynchronizeLogic::handleFileChanged);
    connect(&Singletons::editorManager(), &EditorManager::editorRenamed, this,
        &SynchronizeLogic::handleEditorFileRenamed);
    connect(&Singletons::editorManager(), &EditorManager::viewportSizeChanged,
        this, &SynchronizeLogic::handleViewportSizeChanged);

    connect(&Singletons::inputState(), &InputState::frameIndexChanged, this,
        &SynchronizeLogic::invalidateRenderSession);
    connect(&Singletons::inputState(), &InputState::timeChanged, this,
        &SynchronizeLogic::invalidateRenderSession);
    connect(&Singletons::inputState(), &InputState::mouseChanged, this,
        &SynchronizeLogic::handleMouseStateChanged);
    connect(&Singletons::inputState(), &InputState::keysChanged, this,
        &SynchronizeLogic::handleKeyboardStateChanged);

    mUpdateEditorsTimer->start(100);
    mEvaluationTimer->setTimerType(Qt::PreciseTimer);

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
    if (std::exchange(mCurrentEditorFileName, fileName) != fileName) {
        mProcessSourceTimer->start();

        Q_EMIT currentEditorChanged(fileName);
    }
}

void SynchronizeLogic::setCurrentEditorSourceType(SourceType sourceType)
{
    if (std::exchange(mCurrentEditorSourceType, sourceType) != sourceType)
        mProcessSourceTimer->start();
}

bool SynchronizeLogic::initializeRenderSession()
{
    if (mRenderSession)
        return true;

    const auto sessionRenderer = Singletons::sessionRenderer();
    if (!sessionRenderer)
        return false;

    mRenderSession = RenderSessionBase::create(sessionRenderer);
    if (!mRenderSession)
        return false;

    connect(mRenderSession.get(), &RenderTask::preparing, this,
        &SynchronizeLogic::handlePreparingEvaluation);
    connect(mRenderSession.get(), &RenderTask::updated, this,
        &SynchronizeLogic::handleEvaluated);

    mProcessSource = std::make_unique<ProcessSource>(sessionRenderer);
    connect(mProcessSource.get(), &ProcessSource::outputChanged, this,
        &SynchronizeLogic::outputChanged);

    return true;
}

void SynchronizeLogic::finishEvaluation()
{
    if (mRenderSession)
        mRenderSession->renderer().finish();
}

void SynchronizeLogic::resetRenderSession()
{
    interruptRunningScriptEngines();
    mRenderSession.reset();
    mProcessSource.reset();
    Singletons::defaultScriptEngine().resetMessages();
    Singletons::videoManager().unloadAll();
    Singletons::fileCache().unloadAll();
    Singletons::inputState().reset();
}

void SynchronizeLogic::resetEvaluation()
{
    evaluate(EvaluationType::Reset);
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
    Q_EMIT evaluationModeChanged(mEvaluationMode);

    if (mEvaluationMode == EvaluationMode::Steady) {
        triggerEvaluation(EvaluationType::Steady);
    } else if (mEvaluationMode == EvaluationMode::Automatic) {
        mEvaluationTimer->stop();
        if (mRenderSessionInvalidated)
            triggerEvaluation(EvaluationType::Automatic);
        if (mRenderSession)
            Singletons::sessionModel().setActiveItems(
                mRenderSession->usedItems());
    } else {
        mEvaluationTimer->stop();
        Singletons::sessionModel().setActiveItems({});
    }
}

bool SynchronizeLogic::resetRenderSessionInvalidationState()
{
    if (std::exchange(mRenderSessionInvalidated, false)) {
        if (mEvaluationMode == EvaluationMode::Automatic)
            mEvaluationTimer->stop();
        return true;
    }
    return false;
}

void SynchronizeLogic::handleFileChanged(const QString &fileName)
{
    mModel.forEachFileItem([&](const FileItem &item) {
        if (item.fileName == fileName) {
            auto index = mModel.getIndex(&item);
            Q_EMIT mModel.dataChanged(index, index);
        }
    });

    auto &editorManager = Singletons::editorManager();
    if (editorManager.currentEditorFileName() == fileName)
        mProcessSourceTimer->start();

    editorManager.resetQmlViewsDependingOn(fileName);
}

void SynchronizeLogic::handleItemsModified(const QModelIndex &topLeft,
    const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_UNUSED(bottomRight)
    // ignore ForegroundRole...
    if (roles.empty())
        handleItemModified(topLeft);
}

void SynchronizeLogic::invalidateRenderSession()
{
    mRenderSessionInvalidated = true;
    if (mEvaluationMode == EvaluationMode::Automatic)
        triggerEvaluation(EvaluationType::Automatic, 10);
}

void SynchronizeLogic::handleItemRenamed(const QModelIndex &index,
    const QString &prevName)
{
    Q_ASSERT(index.column() == SessionModel::Name);
    if (auto fileItem = mModel.item<FileItem>(index))
        handleFileItemRenamed(*fileItem, prevName);
}

void SynchronizeLogic::handleItemAdded(const QModelIndex &parent, int first)
{
    invalidateRenderSession();
    Q_EMIT itemAdded(&mModel.getItem(mModel.index(first, 0, parent)));
}

void SynchronizeLogic::handleItemRemoved(const QModelIndex &parent, int first)
{
    invalidateRenderSession();
    Q_EMIT itemRemoved(&mModel.getItem(mModel.index(first, 0, parent)));
}

void SynchronizeLogic::handleItemModified(const QModelIndex &index)
{
    const auto &item = mModel.getItem(index);

    if (auto fileItem = castItem<FileItem>(item))
        if (index.column() == SessionModel::FileName)
            handleFileItemFileChanged(*fileItem);

    if (auto call = castItem<Call>(item))
        if (index.column() == SessionModel::CallType
            && hasDefaultName<Call::CallType>(*call))
            mModel.setData(index, SessionModel::Name,
                getValueName(call->callType));

    if (auto buffer = castItem<Buffer>(item)) {
        mEditorItemsModified.insert(buffer->id);
    } else if (auto block = castItem<Block>(item)) {
        mEditorItemsModified.insert(block->parent->id);
    } else if (auto field = castItem<Field>(item)) {
        mEditorItemsModified.insert(field->parent->parent->id);
    } else if (auto texture = castItem<Texture>(item)) {
        mEditorItemsModified.insert(texture->id);
    }

    if (mRenderSession && mRenderSession->usedItems().contains(item.id)) {
        invalidateRenderSession();
    } else if (index.column() == SessionModel::ScriptExecuteOn) {
        invalidateRenderSession();
    } else if (auto call = castItem<Call>(item)) {
        if (call->checked
            && call->executeOn == Call::ExecuteOn::EveryEvaluation)
            invalidateRenderSession();
    } else if (castItem<Group>(item)) {
        invalidateRenderSession();
    } else if (index.column() == SessionModel::Name
        && (castItem<Binding>(item) || castItem<Attribute>(item))) {
        invalidateRenderSession();
    }

    if (castItem<Session>(item)) {
        mProcessSourceTimer->start();
    }
    Q_EMIT itemModified(&item);
}

void SynchronizeLogic::handleEditorFileRenamed(const QString &prevFileName,
    const QString &fileName)
{
    // update references to untitled file
    if (FileDialog::isUntitled(prevFileName))
        mModel.forEachFileItem([&](const FileItem &item) {
            if (item.fileName == prevFileName) {
                mModel.setData(&item, SessionModel::FileName, fileName);

                // also synchronize shader type
                if (item.type == Item::Type::Shader) {
                    auto &editorManager = Singletons::editorManager();
                    if (auto editor = editorManager.getSourceEditor(fileName))
                        mModel.setData(&item, SessionModel::ShaderType,
                            getShaderType(editor->sourceType()));
                }
            }
        });
}

void SynchronizeLogic::handleFileItemFileChanged(const FileItem &item)
{
    // update item name
    auto name = FileDialog::getFileTitle(item.fileName);
    if (name.isEmpty()) {
        // only reset to type name when it currently has a filename
        if (FileDialog::getFileExtension(item.name).isEmpty())
            return;
        name = mModel.getTypeName(item.type);
    }
    if (name != item.name)
        mModel.setData(&item, SessionModel::Name, name);
}

void SynchronizeLogic::handleFileItemRenamed(const FileItem &item,
    const QString &prevName)
{
    // change filename only when item name previously matched the filename
    // and new name also has a suffix
    if (item.fileName.isEmpty()
        || FileDialog::getFileTitle(item.fileName) != prevName
        || FileDialog::getFileTitle(item.fileName) == item.name
        || QFileInfo(item.name).suffix().isEmpty())
        return;

    // do not rename file when changing name to/from sequence name
    if (FileDialog::isSequenceFileName(item.name)
        != FileDialog::isSequenceFileName(prevName)) {
        const auto fileName = toNativeCanonicalFilePath(
            QFileInfo(item.fileName).dir().filePath(item.name));
        mModel.setData(&item, SessionModel::FileName, fileName);
    }

    mModel.beginUndoMacro("Update filename");

    const auto prevFileName = item.fileName;
    if (!FileDialog::isEmptyOrUntitled(item.fileName)) {
        const auto fileName = toNativeCanonicalFilePath(
            QFileInfo(item.fileName).dir().filePath(item.name));
        const auto file = QFileInfo(fileName);
        const auto prevFile = QFileInfo(prevFileName);

        const auto isDuplicate = [](const QFileInfo &a, const QFileInfo &b) {
            if (a.fileName().compare(b.fileName(), Qt::CaseInsensitive) == 0)
                return false;
            if (!a.exists() || !b.exists() || a.size() != b.size())
                return false;
            return QFile(a.fileName()).readAll()
                == QFile(b.fileName()).readAll();
        };

        const auto mergeRenameOrCopy = [&]() {
            if (isDuplicate(prevFile, file))
                return QFile::remove(prevFileName);

            auto fileReferencedByOtherItem = false;
            mModel.forEachFileItem([&](const FileItem &other) {
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
            mModel.setData(&item, SessionModel::Name, name);
            return;
        }

        // update item filename
        mModel.setData(&item, SessionModel::FileName, fileName);
    } else {
        // update item filename
        const auto fileName =
            FileDialog::generateNextUntitledFileName(item.name);
        mModel.setData(&item, SessionModel::FileName, fileName);
    }

    mModel.endUndoMacro();

    // rename editor filenames
    Singletons::editorManager().renameEditors(prevFileName, item.fileName);
}

void SynchronizeLogic::triggerEvaluation(EvaluationType type, int delayMs)
{
    if (!mEvaluationTimer->isActive()) {
        mPendingEvaluationType = type;
        mEvaluationTimer->start(delayMs);
    } else {
        mPendingEvaluationType = std::max(mPendingEvaluationType, type);
    }
}

void SynchronizeLogic::handleEvaluateTimout()
{
    evaluate(mPendingEvaluationType);
}

void SynchronizeLogic::evaluate(EvaluationType evaluationType)
{
    // interrupt script engines when resetting twice
    if (mEvaluationType == EvaluationType::Reset)
        interruptRunningScriptEngines();

    // reset messages of default script engine
    const auto prevMessages = Singletons::defaultScriptEngine().resetMessages();

    const auto sessionRenderer = Singletons::sessionRenderer();
    if (mRenderSession && &mRenderSession->renderer() != sessionRenderer.get())
        resetRenderSession();

    mEvaluationType = std::max(mEvaluationType, evaluationType);
    if (initializeRenderSession())
        mRenderSession->update();
}

void SynchronizeLogic::handlePreparingEvaluation(bool &itemsChanged,
    EvaluationType &evaluationType)
{
    Singletons::fileCache().updateFromEditors();
    Q_EMIT waitingForSync();

    itemsChanged = mRenderSessionInvalidated;
    evaluationType = mEvaluationType;

    Singletons::inputState().update(mEvaluationType);
    Singletons::videoManager().seek(Singletons::inputState().time());

    mEvaluationType = EvaluationType::Steady;
    mRenderSessionInvalidated = false;
    mEvaluationTimer->stop();
}

void SynchronizeLogic::handleEvaluated()
{
    Singletons::fileCache().updateFromEditors();

    if (mEvaluationMode != EvaluationMode::Paused && mRenderSession)
        Singletons::sessionModel().setActiveItems(mRenderSession->usedItems());

    if (mEvaluationMode == EvaluationMode::Steady)
        triggerEvaluation(EvaluationType::Steady);
}

void SynchronizeLogic::updateEditors()
{
    for (auto itemId : std::as_const(mEditorItemsModified))
        updateEditor(itemId, false);
    mEditorItemsModified.clear();
}

void SynchronizeLogic::updateEditor(ItemId itemId, bool activated)
{
    auto &editors = Singletons::editorManager();
    if (auto texture = mModel.findItem<Texture>(itemId)) {
        if (auto editor = editors.getTextureEditor(texture->fileName))
            updateTextureEditor(*texture, *editor);
    } else if (auto buffer = mModel.findItem<Buffer>(itemId)) {
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
    auto width = 0, height = 0, depth = 0, layers = 0;
    evaluateTextureProperties(texture, &width, &height, &depth, &layers);
    editor.setRawFormat({
        texture.target,
        texture.format,
        width,
        height,
        depth,
        layers,
        texture.samples,
    });
}

void SynchronizeLogic::updateBinaryEditor(const Buffer &buffer,
    BinaryEditor &editor)
{
    const auto getDataType = [](Field::DataType type) {
        switch (type) {
        case Field::DataType::Int8:   return BinaryEditor::DataType::Int8;
        case Field::DataType::Int16:  return BinaryEditor::DataType::Int16;
        case Field::DataType::Int32:  return BinaryEditor::DataType::Int32;
        case Field::DataType::Int64:  return BinaryEditor::DataType::Int64;
        case Field::DataType::Uint8:  return BinaryEditor::DataType::Uint8;
        case Field::DataType::Uint16: return BinaryEditor::DataType::Uint16;
        case Field::DataType::Uint32: return BinaryEditor::DataType::Uint32;
        case Field::DataType::Uint64: return BinaryEditor::DataType::Uint64;
        case Field::DataType::Float:  return BinaryEditor::DataType::Float;
        case Field::DataType::Double: return BinaryEditor::DataType::Double;
        }
        return BinaryEditor::DataType::Int8;
    };

    auto blocks = QList<BinaryEditor::Block>();
    for (const auto *item : std::as_const(buffer.items)) {
        const auto &block = static_cast<const Block &>(*item);
        auto fields = QList<BinaryEditor::Field>();
        for (const auto *item : std::as_const(block.items)) {
            const auto &field = static_cast<const Field &>(*item);
            fields.append({ field.name, getDataType(field.dataType),
                field.count, field.padding });
        }
        auto offset = 0, rowCount = 0;
        evaluateBlockProperties(block, &offset, &rowCount);
        blocks.append({ block.name, offset, rowCount, fields });
    }
    editor.setBlocks(blocks);
}

void SynchronizeLogic::handleTextureDeviceDataChanged(ItemId itemId,
    const TextureData &data, const ShareHandle &shareHandle, int samples)
{
    auto &editors = Singletons::editorManager();
    auto &sessionModel = Singletons::sessionModel();

    if (const auto fileItem = sessionModel.findItem<FileItem>(itemId))
        if (auto editor = editors.openTextureEditor(fileItem->fileName, true)) {
            editors.setAutoRaise(false);
            editor->replace(data, false);
            editor->copySharedTexture(shareHandle, samples);
            editors.setAutoRaise(false);
        }
}

void SynchronizeLogic::handleBufferDataChanged(ItemId itemId,
    const QByteArray &data)
{
    auto &editors = Singletons::editorManager();
    auto &sessionModel = Singletons::sessionModel();

    if (auto fileItem = sessionModel.findItem<FileItem>(itemId))
        if (auto editor = editors.openBinaryEditor(fileItem->fileName, true)) {
            editors.setAutoRaise(false);
            editor->replace(data, false);
            editors.setAutoRaise(true);
        }
}

void SynchronizeLogic::processSource()
{
    if (!mValidateSource && mProcessSourceType.isEmpty()) {
        if (mProcessSource)
            mProcessSource->clearMessages();
        return;
    }

    if (mCurrentEditorFileName.isEmpty()
        || !Singletons::editorManager().getSourceEditor(mCurrentEditorFileName))
        return;

    if (!initializeRenderSession())
        return;

    Singletons::fileCache().updateFromEditors();
    mProcessSource->setFileName(mCurrentEditorFileName);
    mProcessSource->setSourceType(mCurrentEditorSourceType);
    mProcessSource->setValidateSource(mValidateSource);
    mProcessSource->setProcessType(mProcessSourceType);
    mProcessSource->update();
}

void SynchronizeLogic::handleSessionFileNameChanged(const QString &fileName)
{
    Q_ASSERT(FileDialog::isEmptyOrUntitled(fileName)
        || QDir::current() == QFileInfo(fileName).dir());
}

void SynchronizeLogic::handleMouseStateChanged()
{
    if (mRenderSession && mRenderSession->usesMouseState())
        if (mEvaluationMode == EvaluationMode::Automatic)
            triggerEvaluation(EvaluationType::Steady);
}

void SynchronizeLogic::handleKeyboardStateChanged()
{
    if (mRenderSession && mRenderSession->usesKeyboardState())
        if (mEvaluationMode == EvaluationMode::Automatic)
            triggerEvaluation(EvaluationType::Steady);
}

void SynchronizeLogic::handleViewportSizeChanged(const QString &fileName)
{
    if (mRenderSession && mRenderSession->usesViewportSize(fileName))
        invalidateRenderSession();
}

void SynchronizeLogic::evaluateBlockProperties(const Block &block, int *offset,
    int *rowCount)
{
    if (initializeRenderSession())
        mRenderSession->evaluateBlockProperties(block, offset, rowCount, false);
}

void SynchronizeLogic::evaluateTextureProperties(const Texture &texture,
    int *width, int *height, int *depth, int *layers)
{
    if (initializeRenderSession())
        mRenderSession->evaluateTextureProperties(texture, width, height, depth,
            layers, false);
}

void SynchronizeLogic::evaluateTargetProperties(const Target &target,
    int *width, int *height, int *layers)
{
    if (initializeRenderSession())
        mRenderSession->evaluateTargetProperties(target, width, height, layers,
            false);
}
