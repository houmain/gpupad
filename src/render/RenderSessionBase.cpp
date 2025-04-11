
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
    RendererPtr renderer, const QString &basePath)
{
    auto session = std::unique_ptr<RenderSessionBase>();
    switch (renderer->api()) {
    case RenderAPI::OpenGL:
        return std::make_unique<GLRenderSession>(renderer, basePath);
    case RenderAPI::Vulkan:
        return std::make_unique<VKRenderSession>(renderer, basePath);
    }
    Q_UNREACHABLE();
    return {};
}

RenderSessionBase::RenderSessionBase(RendererPtr renderer,
    const QString &basePath, QObject *parent)
    : RenderTask(std::move(renderer), parent)
    , mBasePath(basePath)
{
}

RenderSessionBase::~RenderSessionBase() = default;

void RenderSessionBase::prepare(bool itemsChanged,
    EvaluationType evaluationType)
{
    Q_ASSERT(onMainThread());
    mItemsChanged = itemsChanged;
    mEvaluationType = evaluationType;
    mPrevMessages.swap(mMessages);
    mMessages.clear();

    if (mScriptSession)
        mScriptSession->updateInputState();
    else
        mEvaluationType = EvaluationType::Reset;

    if (mItemsChanged || mEvaluationType == EvaluationType::Reset)
        mSessionModelCopy = Singletons::sessionModel();
}

void RenderSessionBase::configure()
{
    Q_ASSERT(!onMainThread());
    if (mEvaluationType == EvaluationType::Reset)
        mScriptSession.reset(new ScriptSession(this));

    mScriptSession->beginSessionUpdate();

    // collect items to evaluate, since doing so can modify list
    auto itemsToEvaluate = QVector<const Item *>();
    mSessionModelCopy.forEachItem([&](const Item &item) {
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
    Q_ASSERT(onMainThread());
    mScriptSession->endSessionUpdate();

    if (mEvaluationType == EvaluationType::Automatic)
        Singletons::synchronizeLogic().cancelAutomaticRevalidation();

    mMessages += mScriptSession->resetMessages();
}

void RenderSessionBase::finish()
{
    Q_ASSERT(onMainThread());
    auto &editors = Singletons::editorManager();

    editors.setAutoRaise(false);

    for (auto itemId : mModifiedTextures.keys())
        if (auto fileItem =
                castItem<FileItem>(mSessionModelCopy.findItem(itemId)))
            if (auto editor = editors.openTextureEditor(fileItem->fileName))
                editor->replace(mModifiedTextures[itemId], false);
    mModifiedTextures.clear();

    for (auto itemId : mModifiedBuffers.keys())
        if (auto fileItem =
                castItem<FileItem>(mSessionModelCopy.findItem(itemId)))
            if (auto editor = editors.openBinaryEditor(fileItem->fileName))
                editor->replace(mModifiedBuffers[itemId], false);
    mModifiedBuffers.clear();

    editors.setAutoRaise(true);

    mPrevMessages.clear();

    QMutexLocker lock{ &mUsedItemsCopyMutex };
    mUsedItemsCopy = mUsedItems;
}

void RenderSessionBase::release()
{
    mScriptSession.reset();
}

const Session &RenderSessionBase::session() const
{
    return mSessionModelCopy.sessionItem();
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

int RenderSessionBase::getBufferSize(const Buffer &buffer)
{
    auto size = 1;
    for (const Item *item : buffer.items) {
        const auto &block = *static_cast<const Block *>(item);
        auto offset = 0, rowCount = 0;
        evaluateBlockProperties(block, &offset, &rowCount);
        size = std::max(size, offset + rowCount * getBlockStride(block));
    }
    return size;
}

void RenderSessionBase::evaluateBlockProperties(const Block &block, int *offset,
    int *rowCount)
{
    Q_ASSERT(offset && rowCount);
    dispatchToRenderThread([&]() {
        if (!mScriptSession)
            return;
        auto &engine = mScriptSession->engine();
        *offset = engine.evaluateInt(block.offset, block.id, mMessages);
        *rowCount = engine.evaluateInt(block.rowCount, block.id, mMessages);
    });
}

void RenderSessionBase::evaluateTextureProperties(const Texture &texture,
    int *width, int *height, int *depth, int *layers)
{
    Q_ASSERT(width && height && depth && layers);
    dispatchToRenderThread([&]() {
        if (!mScriptSession)
            return;
        auto &engine = mScriptSession->engine();
        *width = engine.evaluateInt(texture.width, texture.id, mMessages);
        *height = engine.evaluateInt(texture.height, texture.id, mMessages);
        *depth = engine.evaluateInt(texture.depth, texture.id, mMessages);
        *layers = engine.evaluateInt(texture.layers, texture.id, mMessages);
    });
}

void RenderSessionBase::evaluateTargetProperties(const Target &target,
    int *width, int *height, int *layers)
{
    Q_ASSERT(width && height && layers);
    dispatchToRenderThread([&]() {
        if (!mScriptSession)
            return;
        auto &engine = mScriptSession->engine();
        *width = engine.evaluateInt(target.defaultWidth, target.id, mMessages);
        *height =
            engine.evaluateInt(target.defaultHeight, target.id, mMessages);
        *layers =
            engine.evaluateInt(target.defaultLayers, target.id, mMessages);
    });
}
