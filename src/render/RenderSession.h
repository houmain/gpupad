#ifndef RENDERSESSION_H
#define RENDERSESSION_H

#include "RenderTask.h"
#include "MessageList.h"
#include "TextureData.h"
#include <QMutex>
#include <QMap>
#include <memory>

class ScriptEngine;
class GpupadScriptObject;
class InputScriptObject;
class QOpenGLTimerQuery;

class RenderSession final : public RenderTask
{
    Q_OBJECT
public:
    explicit RenderSession(QObject *parent = nullptr);
    ~RenderSession() override;

    QSet<ItemId> usedItems() const override;

private:
    struct CommandQueue;
    struct TimerQueries;

    struct GroupIteration {
        int iterations;
        int commandQueueBeginIndex;
        int iterationsLeft;
    };

    void prepare(bool itemsChanged,
        EvaluationType evaluationType) override;
    void render() override;
    void finish() override;
    void release() override;

    void reuseUnmodifiedItems();
    void executeCommandQueue();
    void setNextCommandQueueIndex(int index);
    void downloadModifiedResources();
    void outputTimerQueries();
    bool updatingPreviewTextures() const;

    QScopedPointer<ScriptEngine> mScriptEngine;
    GpupadScriptObject *mGpupadScriptObject{ };
    InputScriptObject *mInputScriptObject{ };
    QScopedPointer<CommandQueue> mCommandQueue;
    QScopedPointer<CommandQueue> mPrevCommandQueue;
    int mNextCommandQueueIndex{ };
    QMap<ItemId, GroupIteration> mGroupIterations;
    QSet<ItemId> mUsedItems;
    QMap<ItemId, TextureData> mModifiedTextures;
    QMap<ItemId, QByteArray> mModifiedBuffers;
    QList<std::pair<ItemId, std::shared_ptr<const QOpenGLTimerQuery>>> mTimerQueries;
    MessagePtrSet mMessages;
    MessagePtrSet mPrevMessages;
    MessagePtrSet mTimerMessages;
    bool mItemsChanged{ };
    EvaluationType mEvaluationType{ };

    mutable QMutex mUsedItemsCopyMutex;
    QSet<ItemId> mUsedItemsCopy;
};

#endif // RENDERSESSION_H
