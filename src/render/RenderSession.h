#ifndef RENDERSESSION_H
#define RENDERSESSION_H

#include "RenderTask.h"

class ScriptEngine;

class RenderSession : public RenderTask
{
public:
    explicit RenderSession(QObject *parent = nullptr);
    ~RenderSession();

    QSet<ItemId> usedItems() const override { return mUsedItems; }

private:
    struct CommandQueue;
    struct TimerQueries;

    void prepare(bool itemsChanged, bool manualEvaluation) override;
    void render() override;
    void finish() override;
    void release() override;

    QScopedPointer<ScriptEngine> mScriptEngine;
    QScopedPointer<CommandQueue> mCommandQueue;
    QScopedPointer<CommandQueue> mPrevCommandQueue;
    QScopedPointer<TimerQueries> mTimerQueries;
    QSet<ItemId> mUsedItems;
    QList<std::pair<QString, QImage>> mModifiedImages;
    QList<std::pair<QString, QByteArray>> mModifiedBuffers;
};

#endif // RENDERSESSION_H
