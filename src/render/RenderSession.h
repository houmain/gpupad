#ifndef RENDERSESSION_H
#define RENDERSESSION_H

#include "RenderTask.h"
#include "MessageList.h"
#include <QMutex>
#include <QMap>

class ScriptEngine;

class RenderSession : public RenderTask
{
    Q_OBJECT
public:
    explicit RenderSession(QObject *parent = nullptr);
    ~RenderSession() override;

    QSet<ItemId> usedItems() const override;

private:
    struct CommandQueue;
    struct TimerQueries;

    void prepare(bool itemsChanged, bool manualEvaluation) override;
    void render() override;
    void finish(bool steadyEvaluation) override;
    void release() override;

    void reuseUnmodifiedItems();
    void evaluateScripts();
    void executeCommandQueue();
    void downloadModifiedResources();
    void outputTimerQueries();

    QScopedPointer<ScriptEngine> mScriptEngine;
    QScopedPointer<CommandQueue> mCommandQueue;
    QScopedPointer<CommandQueue> mPrevCommandQueue;
    QSet<ItemId> mUsedItems;
    QMap<ItemId, QImage> mModifiedImages;
    QMap<ItemId, QByteArray> mModifiedBuffers;
    MessagePtrSet mMessages;
    MessagePtrSet mPrevMessages;

    mutable QMutex mUsedItemsCopyMutex;
    QSet<ItemId> mUsedItemsCopy;
    QSet<ItemId> mUpdatingPreviewTexture;
};

#endif // RENDERSESSION_H
