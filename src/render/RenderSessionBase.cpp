
#include "RenderSessionBase.h"
#include "FileCache.h"
#include "Settings.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "editors/EditorManager.h"
#include "editors/binary/BinaryEditor.h"
#include "editors/texture/TextureEditor.h"
#include "opengl/GLRenderSession.h"
#include "scripting/ScriptEngine.h"
#include "scripting/ScriptSession.h"
#include "session/SessionModel.h"
#include "vulkan/VKRenderSession.h"
#include "vulkan/VKRenderer.h"

std::unique_ptr<RenderSessionBase> RenderSessionBase::create(
    RendererPtr renderer)
{
    switch (renderer->api()) {
    case RenderAPI::OpenGL:
        return std::make_unique<GLRenderSession>(std::move(renderer));
    case RenderAPI::Vulkan:
        return std::make_unique<VKRenderSession>(std::move(renderer));
    }
    return {};
}

RenderSessionBase::RenderSessionBase(RendererPtr renderer, QObject *parent)
    : RenderTask(std::move(renderer), parent)
{
}

RenderSessionBase::~RenderSessionBase() = default;

void RenderSessionBase::prepare(bool itemsChanged,
    EvaluationType evaluationType)
{
    mItemsChanged = itemsChanged;
    mEvaluationType = evaluationType;
    mPrevMessages.swap(mMessages);
    mMessages.clear();

    if (!mScriptSession)
        mEvaluationType = EvaluationType::Reset;

    if (mItemsChanged || mEvaluationType == EvaluationType::Reset)
        mSessionCopy = Singletons::sessionModel();

    if (mEvaluationType == EvaluationType::Reset)
        mScriptSession.reset(new ScriptSession(SourceType::JavaScript));

    mScriptSession->prepare();
}

void RenderSessionBase::configure()
{
    auto &session = mSessionCopy;
    mScriptSession->beginSessionUpdate(&session);

    // collect items to evaluate, since doing so can modify list
    auto itemsToEvaluate = QVector<const Item *>();
    session.forEachItem([&](const Item &item) {
        if (auto script = castItem<Script>(item)) {
            if (shouldExecute(script->executeOn, mEvaluationType))
                itemsToEvaluate.append(script);
        } else if (auto binding = castItem<Binding>(item)) {
            if (binding->bindingType == Binding::BindingType::Uniform)
                itemsToEvaluate.append(binding);
        }
    });

    auto &scriptEngine = mScriptSession->engine();
    for (const auto *item : itemsToEvaluate) {
        if (auto script = castItem<Script>(item)) {
            auto source = QString();
            if (Singletons::fileCache().getSource(script->fileName, &source))
                scriptEngine.evaluateScript(source, script->fileName,
                    mMessages);
        } else if (auto binding = castItem<Binding>(item)) {
            // set global in script state
            auto values = scriptEngine.evaluateValues(binding->values,
                binding->id, mMessages);
            scriptEngine.setGlobal(binding->name, values);
        }
    }
}

void RenderSessionBase::configured()
{
    mScriptSession->endSessionUpdate();

    if (mEvaluationType == EvaluationType::Automatic)
        Singletons::synchronizeLogic().cancelAutomaticRevalidation();

    mMessages += mScriptSession->resetMessages();
}

void RenderSessionBase::finish()
{
    auto &editors = Singletons::editorManager();
    auto &session = Singletons::sessionModel();

    editors.setAutoRaise(false);

    for (auto itemId : mModifiedTextures.keys())
        if (auto fileItem = castItem<FileItem>(session.findItem(itemId)))
            if (auto editor = editors.openTextureEditor(fileItem->fileName))
                editor->replace(mModifiedTextures[itemId], false);
    mModifiedTextures.clear();

    for (auto itemId : mModifiedBuffers.keys())
        if (auto fileItem = castItem<FileItem>(session.findItem(itemId)))
            if (auto editor = editors.openBinaryEditor(fileItem->fileName))
                editor->replace(mModifiedBuffers[itemId], false);
    mModifiedBuffers.clear();

    editors.setAutoRaise(true);

    mPrevMessages.clear();

    QMutexLocker lock{ &mUsedItemsCopyMutex };
    mUsedItemsCopy = mUsedItems;
}

QSet<ItemId> RenderSessionBase::usedItems() const
{
    QMutexLocker lock{ &mUsedItemsCopyMutex };
    return mUsedItemsCopy;
}

bool RenderSessionBase::usesMouseState() const
{
    return (mScriptSession && mScriptSession->usesMouseState());
}

bool RenderSessionBase::usesKeyboardState() const
{
    return (mScriptSession && mScriptSession->usesKeyboardState());
}

bool RenderSessionBase::updatingPreviewTextures() const
{
    return (!mItemsChanged && mEvaluationType == EvaluationType::Steady);
}

void RenderSessionBase::setNextCommandQueueIndex(size_t index)
{
    mNextCommandQueueIndex = index;
}
