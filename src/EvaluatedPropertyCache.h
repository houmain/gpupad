#pragma once

#include "MessageList.h"
#include "session/Item.h"
#include <QMap>
#include <QMutex>

using ScriptEnginePtr = std::shared_ptr<class ScriptEngine>;

class EvaluatedPropertyCache
{
public:
    EvaluatedPropertyCache();
    ~EvaluatedPropertyCache();

    void evaluateBlockProperties(const Block &block, int *offset, int *rowCount,
        ScriptEngine *scriptEngine = nullptr);

    void evaluateTextureProperties(const Texture &texture, int *width,
        int *height, int *depth, int *layers,
        ScriptEngine *scriptEngine = nullptr);

    void evaluateTargetProperties(const Target &target, int *width, int *height,
        int *layers, ScriptEngine *scriptEngine = nullptr);

    void invalidate(ItemId itemId);

private:
    Q_DISABLE_COPY(EvaluatedPropertyCache)
    ScriptEngine *defaultScriptEngine();

    QMutex mMutex;
    ScriptEnginePtr mDefaultScriptEngine;
    QMap<ItemId, QList<int>> mEvaluatedProperties;
    QMap<ItemId, MessagePtrSet> mMessages;
};
