#pragma once

#include "MessageList.h"
#include "session/Item.h"
#include <QMap>
#include <QMutex>

class ScriptEngine;

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

    void invalidate(ItemId itemId);

private:
    Q_DISABLE_COPY(EvaluatedPropertyCache)

    QMutex mMutex;
    QScopedPointer<ScriptEngine> mDefaultScriptEngine;
    QMap<ItemId, QList<int>> mEvaluatedProperties;
    QMap<ItemId, MessagePtrSet> mMessages;
};
