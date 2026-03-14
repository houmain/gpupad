
#include "RenderSessionBase.h"
#include "FileCache.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "scripting/ScriptEngine.h"
#include "scripting/ScriptSession.h"
#include "session/SessionModel.h"
#include "opengl/GLRenderSession.h"
#include "vulkan/VKRenderSession.h"

#if defined(_WIN32)
#  include "direct3d/D3DRenderSession.h"
#endif

std::unique_ptr<RenderSessionBase> RenderSessionBase::create(
    RendererPtr renderer, const QString &basePath)
{
    auto session = std::unique_ptr<RenderSessionBase>();
    switch (renderer->api()) {
    case RenderAPI::OpenGL:
        return std::make_unique<GLRenderSession>(renderer, basePath);
    case RenderAPI::Vulkan:
        return std::make_unique<VKRenderSession>(renderer, basePath);
    case RenderAPI::Direct3D:
#if defined(_WIN32)
        return std::make_unique<D3DRenderSession>(renderer, basePath);
#endif
        break;
    }
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

    if (itemsChanged)
        invalidateCachedProperties();

    if (mScriptSession) {
        mScriptSession->update();
    } else {
        mEvaluationType = EvaluationType::Reset;
    }
    if (mItemsChanged || mEvaluationType == EvaluationType::Reset) {
        mUsedItems.clear();
        mSessionModelCopy = Singletons::sessionModel();
    }
}

void RenderSessionBase::configure()
{
    Q_ASSERT(!onMainThread());

    if (mEvaluationType == EvaluationType::Reset) {
        mScriptSession.reset(new ScriptSession(this));
    } else {
        mScriptSession->resetMessages();
    }

    mScriptSession->beginSessionUpdate();
    mBindingValues.clear();

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
    for (const auto *item : std::as_const(itemsToEvaluate)) {
        if (auto script = castItem<Script>(item)) {
            auto source = QString();
            if (Singletons::fileCache().getSource(script->fileName, &source))
                scriptEngine.evaluateScript(source, script->fileName);
        } else if (auto binding = castItem<Binding>(item)) {
            // evaluate bindings with script objects, since they are setting globals
            evaluateBindingValues(*binding, scriptEngine);
        }
    }
}

void RenderSessionBase::evaluateBindingValues(const Binding &binding,
    ScriptEngine &scriptEngine)
{
    auto &values = mBindingValues[binding.id];
    if (values.isEmpty()) {
        values = scriptEngine.evaluateValues(binding.values, binding.id);

        // set global in script state
        scriptEngine.setGlobal(binding.name, values);
    }
}

void RenderSessionBase::configured()
{
    Q_ASSERT(onMainThread());
    if (mScriptSession)
        mScriptSession->endSessionUpdate();

    if (mEvaluationType != EvaluationType::Steady
        && Singletons::synchronizeLogic().resetRenderSessionInvalidationState())
        mItemsChanged = true;
}

void RenderSessionBase::release()
{
    if (mScriptSession)
        mScriptSession->resetEngine();
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

bool RenderSessionBase::usesViewportSize(const QString &fileName) const
{
    return (mScriptSession && mScriptSession->usesViewportSize(fileName));
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

void RenderSessionBase::invalidateCachedProperties()
{
    auto lock = QMutexLocker(&mPropertyCacheMutex);
    mPropertyCache.clear();
}

QList<int> RenderSessionBase::getCachedProperties(ItemId itemId)
{
    auto lock = QMutexLocker(&mPropertyCacheMutex);
    return mPropertyCache[itemId];
}

void RenderSessionBase::updateCachedProperties(ItemId itemId, QList<int> values)
{
    auto lock = QMutexLocker(&mPropertyCacheMutex);
    mPropertyCache[itemId] = values;
}

std::optional<size_t> RenderSessionBase::addTimeQuery(ItemId callId)
{
    if (mTimeQueryCallIds.size() >= maxTimeQueries)
        return std::nullopt;
    mTimeQueryCallIds.push_back(callId);
    return mTimeQueryCallIds.size() - 1;
}

void RenderSessionBase::evaluateBlockProperties(const Block &block, int *offset,
    int *rowCount, bool cached)
{
    if (auto values = getCachedProperties(block.id);
        cached && values.size() == 2) {
        *offset = values[0];
        *rowCount = values[1];
        return;
    }

    const auto evaluate = [&](ScriptEngine &engine) {
        Q_ASSERT(offset && rowCount);
        *offset = engine.evaluateInt(block.offset, block.id);
        *rowCount = engine.evaluateInt(block.rowCount, block.id);
    };
    if (mScriptSession) {
        dispatchToRenderThread([&]() { evaluate(mScriptSession->engine()); });
        updateCachedProperties(block.id, { *offset, *rowCount });
    } else {
        evaluate(Singletons::defaultScriptEngine());
    }
}

void RenderSessionBase::evaluateTextureProperties(const Texture &texture,
    int *width, int *height, int *depth, int *layers, bool cached)
{
    if (auto values = getCachedProperties(texture.id);
        cached && values.size() == 4) {
        *width = values[0];
        *height = values[1];
        *depth = values[2];
        *layers = values[3];
        return;
    }

    const auto evaluate = [&](ScriptEngine &engine) {
        Q_ASSERT(width && height && depth && layers);
        *width = engine.evaluateInt(texture.width, texture.id);
        *height = engine.evaluateInt(texture.height, texture.id);
        *depth = engine.evaluateInt(texture.depth, texture.id);
        *layers = engine.evaluateInt(texture.layers, texture.id);
    };
    if (mScriptSession) {
        dispatchToRenderThread([&]() { evaluate(mScriptSession->engine()); });
        updateCachedProperties(texture.id,
            { *width, *height, *depth, *layers });
    } else {
        evaluate(Singletons::defaultScriptEngine());
    }
}

void RenderSessionBase::evaluateTargetProperties(const Target &target,
    int *width, int *height, int *layers, bool cached)
{
    if (auto values = getCachedProperties(target.id);
        cached && values.size() == 3) {
        *width = values[0];
        *height = values[1];
        *layers = values[2];
        return;
    }

    const auto evaluate = [&](ScriptEngine &engine) {
        Q_ASSERT(width && height && layers);
        *width = engine.evaluateInt(target.defaultWidth, target.id);
        *height = engine.evaluateInt(target.defaultHeight, target.id);
        *layers = engine.evaluateInt(target.defaultLayers, target.id);
    };
    if (mScriptSession) {
        dispatchToRenderThread([&]() { evaluate(mScriptSession->engine()); });
        updateCachedProperties(target.id, { *width, *height, *layers });
    } else {
        evaluate(Singletons::defaultScriptEngine());
    }
}

void RenderSessionBase::obtainTimeQueryResults()
{
    // TODO: remove when message list performance issue is resolved
    if (updatingPreviewTextures()) {
        resetTimeQueries(0);
        mTimeQueryCallIds.clear();
        return;
    }

    mTimeQueryMessages.clear();
    auto total = std::chrono::duration<double>::zero();
    for (const auto &duration : resetTimeQueries(timeQueryCount())) {
        const auto itemId = mTimeQueryCallIds[mTimeQueryMessages.size()];
        mTimeQueryMessages += MessageList::insert(itemId,
            MessageType::CallDuration, formatDuration(duration), false);
        total += duration;
    }
    mTimeQueryCallIds.clear();

    if (mTimeQueryMessages.size() > 1)
        mTimeQueryMessages += MessageList::insert(0, MessageType::TotalDuration,
            formatDuration(total), false);
}
