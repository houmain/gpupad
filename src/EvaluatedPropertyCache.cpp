
#include "EvaluatedPropertyCache.h"
#include "scripting/ScriptEngineJavaScript.h"

EvaluatedPropertyCache::EvaluatedPropertyCache() { }

EvaluatedPropertyCache::~EvaluatedPropertyCache() = default;

ScriptEngine *EvaluatedPropertyCache::defaultScriptEngine()
{
    if (!mDefaultScriptEngine)
        mDefaultScriptEngine = std::make_unique<ScriptEngineJavaScript>();
    return mDefaultScriptEngine.get();
}

// when a script engine is passed then evaluate and update cache,
// otherwise load from cache or evaluate using default engine
void EvaluatedPropertyCache::evaluateBlockProperties(const Block &block,
    int *offset, int *rowCount, ScriptEngine *scriptEngine)
{
    QMutexLocker lock{ &mMutex };
    Q_ASSERT(offset && rowCount);

    const auto updateCache = (scriptEngine != nullptr);
    if (!updateCache) {
        const auto it = mEvaluatedProperties.find(block.id);
        if (it != mEvaluatedProperties.end()) {
            *offset = (*it)[0];
            *rowCount = (*it)[1];
        } else {
            scriptEngine = defaultScriptEngine();
        }
    }
    if (scriptEngine) {
        auto &messages = mMessages[block.id];
        messages.clear();
        *offset = scriptEngine->evaluateInt(block.offset, block.id, messages);
        *rowCount =
            scriptEngine->evaluateInt(block.rowCount, block.id, messages);
    }
    if (updateCache) {
        mEvaluatedProperties[block.id] = { *offset, *rowCount };
    }
}

void EvaluatedPropertyCache::evaluateTextureProperties(const Texture &texture,
    int *width, int *height, int *depth, int *layers,
    ScriptEngine *scriptEngine)
{
    QMutexLocker lock{ &mMutex };
    Q_ASSERT(width && height && depth && layers);

    const auto updateCache = (scriptEngine != nullptr);
    if (!updateCache) {
        const auto it = mEvaluatedProperties.find(texture.id);
        if (it != mEvaluatedProperties.end()) {
            *width = (*it)[0];
            *height = (*it)[1];
            *depth = (*it)[2];
            *layers = (*it)[3];
        } else {
            scriptEngine = defaultScriptEngine();
        }
    }
    if (scriptEngine) {
        auto &messages = mMessages[texture.id];
        messages.clear();
        *width = scriptEngine->evaluateInt(texture.width, texture.id, messages);
        *height =
            scriptEngine->evaluateInt(texture.height, texture.id, messages);
        *depth = scriptEngine->evaluateInt(texture.depth, texture.id, messages);
        *layers =
            scriptEngine->evaluateInt(texture.layers, texture.id, messages);
    }
    if (updateCache) {
        mEvaluatedProperties[texture.id] = { *width, *height, *depth, *layers };
    }
}

void EvaluatedPropertyCache::invalidate(ItemId itemId)
{
    QMutexLocker lock{ &mMutex };
    mEvaluatedProperties.remove(itemId);
    mMessages.remove(itemId);
}
