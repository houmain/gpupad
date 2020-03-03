#ifndef RENDERSESSION_H
#define RENDERSESSION_H

#include "RenderTask.h"
#include "MessageList.h"
#include "TextureData.h"
#include <QMutex>
#include <QMap>
#include <memory>

class ScriptEngine;
class InputScriptObject;
class QOpenGLTimerQuery;

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
    void finish() override;
    void release() override;

    void reuseUnmodifiedItems();
    void executeCommandQueue();
    void downloadModifiedResources();
    void outputTimerQueries();

    InputScriptObject *mInputScriptObject{ };
    QScopedPointer<CommandQueue> mCommandQueue;
    QScopedPointer<CommandQueue> mPrevCommandQueue;
    QScopedPointer<ScriptEngine> mScriptEngine;
    QSet<ItemId> mUsedItems;
    QMap<ItemId, TextureData> mModifiedTextures;
    QMap<ItemId, QByteArray> mModifiedBuffers;
    QMap<ItemId, std::shared_ptr<const QOpenGLTimerQuery>> mTimerQueries;
    MessagePtrSet mMessages;
    MessagePtrSet mPrevMessages;
    bool mItemsChanged{ };
    bool mManualEvaluation{ };

    mutable QMutex mUsedItemsCopyMutex;
    QSet<ItemId> mUsedItemsCopy;
};

#endif // RENDERSESSION_H
